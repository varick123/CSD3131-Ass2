#!/usr/bin/env python3
"""Exercise the local HTTP viewer with the real compiled application, no third-party packages."""
import importlib.util
import http.client
import json
from pathlib import Path
import struct
import socket
import sys
import tempfile
import threading
import unittest
from urllib.error import HTTPError
from urllib.parse import quote
from urllib.request import Request, urlopen

ROOT=Path(__file__).resolve().parents[1]
SPEC=importlib.util.spec_from_file_location('seam_viewer',ROOT/'web/server.py')
VIEWER=importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(VIEWER)
EXE=Path(sys.argv.pop(1)).resolve()

class ViewerTests(unittest.TestCase):
    requests_made=0

    @classmethod
    def setUpClass(cls):
        cls.temporary=tempfile.TemporaryDirectory(prefix='seam-http-test-')
        cls.directory=Path(cls.temporary.name)
        VIEWER.ViewerHandler.log_message=lambda *args: None
        cls.server=VIEWER.ViewerServer(('127.0.0.1',0),EXE,cls.directory)
        cls.thread=threading.Thread(target=cls.server.serve_forever,daemon=True)
        cls.thread.start()
        cls.base=f'http://127.0.0.1:{cls.server.server_address[1]}'
        w,h=16,12
        cls.ppm=b'P6\n16 12\n255\n'+bytes(x for r in range(h) for c in range(w)
            for x in ((31*c+17*r)%256,(11*c+37*r)%256,(43*c+7*r)%256))

    @classmethod
    def tearDownClass(cls):
        cls.server.shutdown();cls.server.server_close();cls.thread.join(timeout=10)
        cls.temporary.cleanup()

    def request(self,path,body=None,headers=None,status=200):
        ViewerTests.requests_made+=1
        req=Request(self.base+path,data=body,headers=headers or {})
        try:
            response=urlopen(req,timeout=30)
        except HTTPError as error:
            response=error
        with response:
            data=response.read();actual=response.status;result_headers=response.headers
        self.assertEqual(actual,status,(path,data[:300]))
        self.assertEqual(result_headers.get('Cache-Control'),'no-store')
        if result_headers.get_content_type()=='application/json':
            data=json.loads(data)
        return data,result_headers

    def upload(self):
        data,_=self.request('/api/upload',self.ppm,{'X-Filename':quote('sample image 测试.ppm')})
        self.assertEqual((data['width'],data['height']),(16,12))
        self.assertEqual(data['filename'],'sample image 测试.ppm')
        preview,_=self.request(data['preview'])
        self.assert_png_size(preview,16,12)
        return data

    def run_settings(self,settings,status=200):
        return self.request('/api/run',json.dumps(settings).encode(),{'Content-Type':'application/json'},status)[0]

    def assert_png_size(self,data,w,h):
        self.assertEqual(data[:8],b'\x89PNG\r\n\x1a\n')
        self.assertEqual(struct.unpack('>II',data[16:24]),(w,h))

    def test_home_health_sample(self):
        home,_=self.request('/')
        self.assertIn(b'Try sample image',home)
        health,_=self.request('/api/health');self.assertEqual(health['status'],'ok')
        sample,_=self.request('/api/sample')
        self.assertEqual(sample,(ROOT/'data/demo/scene_1.png').read_bytes())
        same,_=self.request('/api/sample?path=ignored');self.assertEqual(sample,same)

    def test_individual_methods_and_all_frames(self):
        upload=self.upload()
        for method in ('dp','greedy','graph','all'):
            with self.subTest(method=method):
                result=self.run_settings({'token':upload['token'],'width':14,'height':10,
                    'method':method,'order':'horizontal-first'})
                self.assertEqual(len(result['methods']),3 if method=='all' else 1)
                for entry in result['methods']:
                    self.assertEqual((entry['width'],entry['height']),(14,10))
                    self.assertEqual(len(entry['frames']),4)
                    final,headers=self.request(entry['output_url']+'?download=1')
                    self.assert_png_size(final,14,10)
                    self.assertIn('attachment;',headers.get('Content-Disposition',''))
                    mapped,_=self.request(entry['all_seams_url']);self.assert_png_size(mapped,16,12)
                    for frame in entry['frames']:
                        self.assertIn(frame['axis'],('v','h'))
                        w,h=frame['width'],frame['height']
                        for stage in ('seam','energy','after'):
                            pixels,_=self.request(frame[stage+'_url'])
                            self.assert_png_size(pixels,w-(stage=='after' and frame['axis']=='v'),
                                h-(stage=='after' and frame['axis']=='h'))
                if method=='all':
                    entries={e['method']:e for e in result['methods']}
                    for axis in ('vertical_cost','horizontal_cost'):
                        self.assertEqual(entries['dp'][axis],entries['graph'][axis])
                        self.assertGreaterEqual(entries['greedy'][axis],entries['dp'][axis])

    def test_unchanged_and_invalid_settings(self):
        upload=self.upload()
        settings={'token':upload['token'],'width':16,'height':12,'method':'dp','order':'vertical-first'}
        result=self.run_settings(settings)
        self.assertEqual(result['methods'][0]['frames'],[])
        final,_=self.request(result['methods'][0]['output_url']);self.assert_png_size(final,16,12)
        bad=[{'width':0},{'height':13},{'width':True},{'height':1.5},{'width':'12'},
             {'method':'unknown'},{'order':'unknown'},{'method':[]},{'method':{}},
             {'order':[]},{'order':{}},{'token':'bad'}]
        for override in bad:
            with self.subTest(override=override):
                error=self.run_settings({**settings,**override},400)
                self.assertIn('error',error)
        self.request('/api/run',b'[]',{'Content-Type':'application/json'},400)
        self.request('/api/run',b'{',{'Content-Type':'application/json'},400)
        self.request('/api/run',b'{}',{'Content-Type':'text/plain'},415)

    def test_limits_before_decode_and_concurrency(self):
        upload=self.upload()
        session=self.server.sessions[upload['token']]
        settings={'token':upload['token'],'width':14,'height':10,'method':'dp','order':'vertical-first'}
        session.lock.acquire()
        try:self.run_settings(settings,409)
        finally:session.lock.release()
        old_w,old_h=session.width,session.height
        session.width,session.height=513,128
        try:self.run_settings({**settings,'method':'graph'},400)
        finally:session.width,session.height=old_w,old_h
        # A valid oversized PNG header with no IDAT: metadata must trigger the
        # viewer's dimension limit before the decoder attempts the missing pixels.
        import binascii,zlib
        def chunk(kind,data):
            return struct.pack('>I',len(data))+kind+data+struct.pack('>I',binascii.crc32(kind+data)&0xffffffff)
        huge=b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('>IIBBBBB',9000,2,8,2,0,0,0))+chunk(b'IEND',b'')
        error,_=self.request('/api/upload',huge,{'X-Filename':'large.png'},400)
        self.assertIn('8192',error['error'])

    def test_upload_routes_and_file_boundaries(self):
        self.request('/api/upload',b'',{'X-Filename':'empty.png'},400)
        self.request('/api/upload',b'not an image',{'X-Filename':'broken.png'},422)
        self.request('/api/upload',b'x',{'X-Filename':'no.txt'},400)
        self.request('/missing',status=404)
        self.request('/api/health',headers={'Host':'unrelated.example'},status=403)
        self.request('/api/health',headers={'Origin':'http://unrelated.example'},status=403)
        upload=self.upload()
        self.request(f"/files/{upload['token']}/../../index.html",status=404)
        self.request(f"/files/{upload['token']}/source.ppm",status=404)
        self.request('/files/'+'0'*32+'/preview.png',status=404)

    def test_rejected_upload_consumes_body_before_reply(self):
        # Send headers and body separately, exposing the early-rejection race
        # that can abort a Windows client while it is still sending its body.
        payload=b'x'*4096
        ViewerTests.requests_made+=1
        host,port=self.server.server_address
        headers=(f'POST /api/upload HTTP/1.1\r\nHost: {host}:{port}\r\n'
                 f'Content-Length: {len(payload)}\r\nX-Filename: no.txt\r\n'
                 'Connection: close\r\n\r\n').encode('ascii')
        with socket.create_connection((host,port),timeout=5) as connection:
            connection.sendall(headers)
            connection.settimeout(0.2)
            with self.assertRaises(TimeoutError,msg='Rejected upload replied before consuming its bounded body'):
                connection.recv(1)
            connection.settimeout(5)
            connection.sendall(payload)
            response=http.client.HTTPResponse(connection)
            response.begin()
            self.assertEqual(response.status,400)
            self.assertIn('Choose a raster image',json.loads(response.read())['error'])
            response.close()

if __name__=='__main__':
    result=unittest.TextTestRunner(verbosity=2).run(unittest.defaultTestLoader.loadTestsFromTestCase(ViewerTests))
    print(f'Viewer HTTP requests checked: {ViewerTests.requests_made}')
    raise SystemExit(0 if result.wasSuccessful() else 1)
