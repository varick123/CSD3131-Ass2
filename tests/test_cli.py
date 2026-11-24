"""Standard-library integration checks for the compiled seamcarve executable."""

import csv
import io
import json
import math
import os
import pathlib
import struct
import subprocess
import sys
import tempfile
import zlib


EXE = pathlib.Path(sys.argv[1]).resolve()
CHECKS = 0
INVOCATIONS = 0
RESIZES = 0
SKIPPED = []


def check(condition, message):
    global CHECKS
    CHECKS += 1
    if not condition:
        raise AssertionError(message)


def run(*arguments, success=True):
    global INVOCATIONS
    INVOCATIONS += 1
    command = [str(EXE), *(str(value) for value in arguments)]
    result = subprocess.run(command, capture_output=True, text=True, encoding="utf-8",
                            errors="replace", timeout=8)
    if success:
        check(result.returncode == 0,
              f"Command failed ({result.returncode}): {command!r}\n{result.stderr}")
        check(not result.stderr.strip(), f"Unexpected stderr: {command!r}: {result.stderr}")
    else:
        check(result.returncode != 0, f"Invalid command succeeded: {command!r}")
        check("Error:" in result.stderr, f"Missing useful error: {command!r}: {result.stderr}")
    return result


def run_json(*arguments):
    result = run(*arguments)
    try:
        return json.loads(result.stdout)
    except json.JSONDecodeError as error:
        raise AssertionError(f"Invalid stdout JSON: {result.stdout!r}") from error


def write_ppm(path, image):
    height, width = len(image), len(image[0])
    pixels = bytes(channel for row in image for pixel in row for channel in pixel)
    path.write_bytes(f"P6\n{width} {height}\n255\n".encode("ascii") + pixels)


def write_png(path, width, height, depth, color, rows):
    """Create small codec fixtures directly, without an imaging library."""
    def chunk(kind, data):
        return (struct.pack(">I", len(data)) + kind + data +
                struct.pack(">I", zlib.crc32(kind + data) & 0xFFFFFFFF))

    check(len(rows) == height, "test PNG fixture row count")
    header = struct.pack(">IIBBBBB", width, height, depth, color, 0, 0, 0)
    raw = b"".join(b"\0" + bytes(row) for row in rows)
    path.write_bytes(b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", header) +
                     chunk(b"IDAT", zlib.compress(raw)) + chunk(b"IEND", b""))


def snapshot(directory):
    return {str(path.relative_to(directory)): path.read_bytes()
            for path in directory.rglob("*") if path.is_file()}


def read_png(path):
    data = path.read_bytes()
    check(data[:8] == b"\x89PNG\r\n\x1a\n", f"PNG signature: {path}")
    offset = 8
    compressed = bytearray()
    width = height = channels = None
    while offset < len(data):
        check(offset + 12 <= len(data), f"Truncated PNG chunk: {path}")
        size = struct.unpack_from(">I", data, offset)[0]
        kind = data[offset + 4:offset + 8]
        payload = data[offset + 8:offset + 8 + size]
        check(len(payload) == size and offset + 12 + size <= len(data),
              f"Truncated PNG payload: {path}")
        checksum = struct.unpack_from(">I", data, offset + 8 + size)[0]
        check(zlib.crc32(kind + payload) & 0xFFFFFFFF == checksum,
              f"PNG checksum: {path}")
        if kind == b"IHDR":
            width, height, depth, color, compression, filtering, interlace = struct.unpack(
                ">IIBBBBB", payload)
            check(depth == 8 and color in (2, 6) and compression == 0 and
                  filtering == 0 and interlace == 0, f"Unsupported test PNG: {path}")
            channels = 3 if color == 2 else 4
        elif kind == b"IDAT":
            compressed.extend(payload)
        elif kind == b"IEND":
            break
        offset += size + 12
    check(width is not None and height is not None, f"Missing PNG dimensions: {path}")
    raw = zlib.decompress(compressed)
    stride = width * channels
    check(len(raw) == height * (stride + 1), f"PNG scanline length: {path}")
    previous = bytearray(stride)
    image = []
    position = 0
    for _ in range(height):
        method = raw[position]
        position += 1
        scan = bytearray(raw[position:position + stride])
        position += stride
        check(method in range(5), f"Invalid PNG filter: {path}")
        for index in range(stride):
            left = scan[index - channels] if index >= channels else 0
            above = previous[index]
            upper_left = previous[index - channels] if index >= channels else 0
            if method == 1:
                predictor = left
            elif method == 2:
                predictor = above
            elif method == 3:
                predictor = (left + above) // 2
            elif method == 4:
                estimate = left + above - upper_left
                distances = (abs(estimate - left), abs(estimate - above),
                             abs(estimate - upper_left))
                predictor = (left, above, upper_left)[distances.index(min(distances))]
            else:
                predictor = 0
            scan[index] = (scan[index] + predictor) & 255
        image.append([tuple(scan[index:index + 3]) for index in range(0, stride, channels)])
        previous = scan
    return image


def transpose(image):
    return [list(column) for column in zip(*image)]


def energy(image):
    height, width = len(image), len(image[0])
    gray = [[(299 * red + 587 * green + 114 * blue + 500) // 1000
             for red, green, blue in row] for row in image]
    kx = ((-1, 0, 1), (-2, 0, 2), (-1, 0, 1))
    ky = ((-1, -2, -1), (0, 0, 0), (1, 2, 1))
    result = []
    for row in range(height):
        output = []
        for column in range(width):
            gx = gy = 0
            for dr in range(-1, 2):
                for dc in range(-1, 2):
                    value = gray[min(height - 1, max(0, row + dr))][
                        min(width - 1, max(0, column + dc))]
                    gx += value * kx[dr + 1][dc + 1]
                    gy += value * ky[dr + 1][dc + 1]
            output.append(abs(gx) + abs(gy))
        result.append(output)
    return result


def optimum(grid):
    previous = grid[0][:]
    for row in grid[1:]:
        previous = [value + min(previous[max(0, column - 1):column + 2])
                    for column, value in enumerate(row)]
    return min(previous)


def greedy_columns(grid):
    columns = [min(range(len(grid[0])), key=lambda column: grid[0][column])]
    for row in grid[1:]:
        columns.append(min(range(max(0, columns[-1] - 1),
                                 min(len(row), columns[-1] + 2)),
                           key=lambda column: row[column]))
    return columns


def remove(image, columns):
    return [row[:column] + row[column + 1:]
            for row, column in zip(image, columns)]


def check_times(value, fields, label):
    for field in fields:
        check(isinstance(value[field], (int, float)) and math.isfinite(value[field]) and
              value[field] >= 0, f"{label}: invalid {field}")


def verify_resize(summary, original, method, width, height, order, artifacts=None):
    global RESIZES
    RESIZES += 1
    check(summary["method"] == method, "resize method metadata")
    check((summary["original_width"], summary["original_height"]) ==
          (len(original[0]), len(original)), "original dimension metadata")
    check((summary["width"], summary["height"]) == (width, height), "target dimension metadata")
    check(summary["order"] == order, "axis order metadata")
    check_times(summary, ("energy_ms", "search_ms", "remove_ms", "total_ms"), "resize")
    check(abs(summary["total_ms"] - sum(summary[key] for key in
          ("energy_ms", "search_ms", "remove_ms"))) < 0.0001, "stage timing total")
    current = [row[:] for row in original]
    mapping = [[(row, column) for column in range(len(original[0]))]
               for row in range(len(original))]
    all_seams = [row[:] for row in original]
    expected_count = len(original[0]) - width + len(original) - height
    check(len(summary["steps"]) == expected_count, "one step per removed seam")
    for number, step in enumerate(summary["steps"], 1):
        current_height, current_width = len(current), len(current[0])
        axis = ("h" if current_height > height else "v") if order == "horizontal-first" else (
            "v" if current_width > width else "h")
        check(step["index"] == number and step["axis"] == axis, "step index/axis order")
        check((step["width"], step["height"]) == (current_width, current_height),
              "step reports dimensions before removal")
        oriented = transpose(current) if axis == "h" else current
        oriented_mapping = transpose(mapping) if axis == "h" else mapping
        grid = energy(oriented)
        columns = step["columns"]
        check(len(columns) == len(oriented), "seam length in oriented coordinates")
        check(all(isinstance(column, int) and 0 <= column < len(oriented[0])
                  for column in columns), "seam bounds")
        check(all(abs(left - right) <= 1 for left, right in zip(columns, columns[1:])),
              "seam adjacency")
        check(step["cost"] == sum(grid[row][column] for row, column in enumerate(columns)),
              "step energy recomputed from current pixels")
        if method in ("dp", "graph"):
            check(step["cost"] == optimum(grid), "current seam optimality")
        else:
            check(columns == greedy_columns(grid), "documented greedy local decisions")
        check_times(step, ("energy_ms", "search_ms", "remove_ms", "graph_build_ms", "flow_ms"),
                    "step")
        if method == "graph":
            oh, ow = len(oriented), len(oriented[0])
            check(step["vertices"] == oh * (ow + 1) + 2, "boundary graph vertex count")
            check(step["edges"] == 4 * oh * ow + 2 * oh - 2 * ow,
                  "graph original directed-edge count")
        else:
            check(step["vertices"] == 0 and step["edges"] == 0,
                  "non-graph method has no flow graph")
        if artifacts is not None:
            prefix = artifacts / f"{number:04d}"
            marked = [row[:] for row in oriented]
            for row, column in enumerate(columns):
                marked[row][column] = (255, 32, 64)
                source_row, source_column = oriented_mapping[row][column]
                all_seams[source_row][source_column] = (
                    (40, 160, 255) if axis == "h" else (255, 32, 64))
            if axis == "h":
                marked = transpose(marked)
            check(read_png(pathlib.Path(str(prefix) + "_seam.png")) == marked,
                  "step seam overlay, including horizontal coordinates")
            preview = read_png(pathlib.Path(str(prefix) + "_energy.png"))
            check((len(preview[0]), len(preview)) == (current_width, current_height),
                  "energy preview has current display orientation")
        current = remove(oriented, columns)
        # Remove in oriented rows, then transpose back below. Scanning the old
        # display rows and skipping selected pixels does not preserve row order
        # when a horizontal seam selects different source rows in each column.
        mapping = remove(oriented_mapping, columns)
        if axis == "h":
            current, mapping = transpose(current), transpose(mapping)
        if artifacts is not None:
            check(read_png(pathlib.Path(str(prefix) + "_after.png")) == current,
                  "step output pixels and dimensions")
    check((len(current[0]), len(current)) == (width, height), "replayed final dimensions")
    for key in ("energy_ms", "search_ms", "remove_ms"):
        check(abs(summary[key] - sum(step[key] for step in summary["steps"])) < 0.0001,
              "aggregate step timing")
    if artifacts is not None:
        check(read_png(artifacts / "original.png") == original, "artifact original identity")
        check(read_png(artifacts / "result.png") == current, "artifact final pixel identity")
        check(read_png(artifacts / "all_seams.png") == all_seams,
              "all seams map to correct original-image coordinates")
        colors = {(255, 32, 64), (40, 160, 255)}
        check(sum(pixel in colors for row in all_seams for pixel in row) ==
              len(original) * len(original[0]) - width * height,
              "all-seams overlay marks each removed original pixel exactly once")
        check(len(list(artifacts.glob("????_seam.png"))) == expected_count,
              "one seam artifact per step")
        check(len(list(artifacts.glob("????_energy.png"))) == expected_count,
              "one energy artifact per step")
        check(len(list(artifacts.glob("????_after.png"))) == expected_count,
              "one output artifact per step")
        persisted = json.loads((artifacts / "summary.json").read_text(encoding="utf-8"))
        check(persisted["steps"] == summary["steps"], "artifact summary steps")
    return current


def test_codecs(root):
    # The expected pixels are computed independently of the application's decoder.
    gray_rows = [[0, 100], [200, 255]]
    rgba_rows = [[(255, 0, 0, 0), (255, 0, 0, 128)],
                 [(0, 255, 0, 255), (25, 75, 125, 64)]]
    gray_alpha_rows = [[(10, 0), (10, 128)], [(128, 255), (200, 64)]]
    fixtures = [
        ("gray", 0, [bytes(row) for row in gray_rows],
         [[(value, value, value) for value in row] for row in gray_rows]),
        ("rgba", 6,
         [bytes(channel for pixel in row for channel in pixel) for row in rgba_rows],
         [[tuple((value * alpha + 255 * (255 - alpha) + 127) // 255
                 for value in (red, green, blue)) for red, green, blue, alpha in row]
          for row in rgba_rows]),
        ("gray_alpha", 4,
         [bytes(channel for pixel in row for channel in pixel) for row in gray_alpha_rows],
         [[tuple([(value * alpha + 255 * (255 - alpha) + 127) // 255] * 3)
           for value, alpha in row] for row in gray_alpha_rows]),
    ]
    for label, color, rows, expected in fixtures:
        source, output = root / f"codec_{label}.png", root / f"codec_{label}_out.png"
        write_png(source, 2, 2, 8, color, rows)
        check(run_json("info", "--input", source) == {"width": 2, "height": 2},
              f"{label} PNG metadata")
        result = run_json("resize", "--input", source, "--output", output,
                          "--width", 2, "--height", 2)
        verify_resize(result, expected, "dp", 2, 2, "vertical-first")
        check(read_png(output) == expected, f"{label} normalization/compositing exact pixels")

    unsupported = root / "codec_gray_16bit.png"
    write_png(unsupported, 2, 1, 16, 0, [struct.pack(">HH", 0x1234, 0xABCD)])
    hdr = root / "codec_radiance.hdr"
    hdr.write_bytes(b"#?RADIANCE\nFORMAT=32-bit_rle_rgbe\n\n-Y 1 +X 1\n" +
                    bytes((128, 64, 32, 129)))
    for source in (unsupported, hdr):
        run("info", "--input", source, success=False)
        output = root / f"{source.stem}_rejected.png"
        run("resize", "--input", source, "--output", output, success=False)
        check(not output.exists() and not pathlib.Path(str(output) + ".json").exists(),
              "unsupported bit depth creates no output/sidecar")


def test_destination_safety(root, source, original):
    missing_output_artifacts = root / "missing_output_frames"
    run("resize", "--input", source, "--width", 3,
        "--artifacts", missing_output_artifacts, success=False)
    check(not missing_output_artifacts.exists(), "missing output does not create artifacts")

    reused = root / "reused_frames"
    reused.mkdir()
    (reused / "original.png").write_bytes(b"previous original must remain byte-for-byte intact")
    (reused / "nested").mkdir()
    (reused / "nested" / "sentinel.txt").write_bytes(b"previous run data")
    before = snapshot(reused)
    output = root / "reuse_rejected.png"
    run("resize", "--input", source, "--output", output, "--width", 3,
        "--artifacts", reused, success=False)
    check(snapshot(reused) == before, "nonempty artifact reuse preserves all existing data")
    check(not output.exists(), "artifact preflight precedes output creation")

    comparison = root / "compare_old_graph_frames"
    (comparison / "graph").mkdir(parents=True)
    (comparison / "graph" / "sentinel.txt").write_bytes(b"keep old graph frames")
    (comparison / "original.png").write_bytes(b"keep old original")
    before = snapshot(comparison)
    run("compare", "--input", source, "--output-dir", comparison,
        "--width", 3, "--height", 2, "--repeats", 1, "--steps", success=False)
    check(snapshot(comparison) == before,
          "compare preflights every method artifact directory before any writes")

    contained = root / "compare_contains_source"
    contained.mkdir()
    contained_source = contained / "original.png"
    write_png(contained_source, 5, 4, 8, 2,
              [bytes(channel for pixel in row for channel in pixel) for row in original])
    before = snapshot(contained)
    run("compare", "--input", contained_source, "--output-dir", contained,
        "--width", 3, "--height", 2, "--repeats", 1, success=False)
    check(snapshot(contained) == before, "compare source inside output tree remains intact")

    original_bytes = source.read_bytes()
    run("resize", "--input", source, "--output", source, "--width", 3, success=False)
    check(source.read_bytes() == original_bytes, "resize rejects destructive input/output alias")
    sidecar_output = root / "sidecar_alias.png"
    sidecar_source = pathlib.Path(str(sidecar_output) + ".json")
    write_ppm(sidecar_source, original)
    original_sidecar = sidecar_source.read_bytes()
    run("resize", "--input", sidecar_source, "--output", sidecar_output,
        "--width", 3, success=False)
    check(sidecar_source.read_bytes() == original_sidecar and not sidecar_output.exists(),
          "resize rejects input alias with JSON sidecar before any writes")

    alias_directory = root / "canonical_alias"
    alias_directory.mkdir()
    canonical_alias = alias_directory / ".." / source.name
    run("resize", "--input", source, "--output", canonical_alias,
        "--width", 3, success=False)
    check(source.read_bytes() == original_bytes, "canonical input/output alias preserves source")
    if os.name == "nt":
        case_alias = source.with_name(source.name.swapcase())
        run("resize", "--input", source, "--output", case_alias,
            "--width", 3, success=False)
        check(source.read_bytes() == original_bytes,
              "Windows case-insensitive input/output alias preserves source")

    hard_output = root / "hardlinked_output.png"
    try:
        os.link(source, hard_output)
    except OSError as error:
        code = getattr(error, "winerror", None) or error.errno
        SKIPPED.append(f"3 hardlink alias cases: filesystem disallows fixture creation (error {code})")
    else:
        run("resize", "--input", source, "--output", hard_output,
            "--width", 3, success=False)
        check(source.read_bytes() == original_bytes and hard_output.read_bytes() == original_bytes,
              "hardlink-equivalent output cannot overwrite the source")
        hard_sidecar_output = root / "hardlinked_sidecar.png"
        hard_sidecar = pathlib.Path(str(hard_sidecar_output) + ".json")
        os.link(source, hard_sidecar)
        run("resize", "--input", source, "--output", hard_sidecar_output,
            "--width", 3, success=False)
        check(source.read_bytes() == original_bytes and hard_sidecar.read_bytes() == original_bytes and
              not hard_sidecar_output.exists(), "hardlink-equivalent JSON sidecar preserves source")

        hard_compare = root / "hardlinked_compare"
        hard_compare.mkdir()
        os.link(source, hard_compare / "dp.png")
        before = snapshot(hard_compare)
        run("compare", "--input", source, "--output-dir", hard_compare,
            "--width", 3, "--height", 2, "--repeats", 1, success=False)
        check(snapshot(hard_compare) == before and source.read_bytes() == original_bytes,
              "compare rejects source hardlinked to a generated output before writes")

    input_artifacts = root / "artifacts_contains_source"
    input_artifacts.mkdir()
    embedded_source = input_artifacts / "input.ppm"
    write_ppm(embedded_source, original)
    before = snapshot(input_artifacts)
    run("resize", "--input", embedded_source, "--output", root / "embedded_rejected.png",
        "--width", 3, "--artifacts", input_artifacts, success=False)
    check(snapshot(input_artifacts) == before,
          "input inside artifact tree is rejected without changes")

    invalid_directory = root / "invalid_output_inside_artifacts"
    run("resize", "--input", source, "--output", invalid_directory / "original.png",
        "--width", 3, "--height", 2, "--artifacts", invalid_directory, success=False)
    check(not invalid_directory.exists(), "reserved artifact output path rejected before writes")

    supported_directory = root / "supported_result_output"
    supported_directory.mkdir()  # Existing empty artifact directories are permitted.
    supported_output = supported_directory / "result.png"
    result = run_json("resize", "--input", source, "--output", supported_output,
                      "--width", 3, "--height", 2, "--artifacts", supported_directory)
    expected = verify_resize(result, original, "dp", 3, 2, "vertical-first", supported_directory)
    check(read_png(supported_output) == expected,
          "result.png is the supported output inside an artifact directory")
    check(json.loads(pathlib.Path(str(supported_output) + ".json").read_text(encoding="utf-8")) ==
          result, "supported result output has consistent JSON sidecar")


def main(root):
    for arguments in ((), ("--help",), ("help",)):
        check("seamcarve resize" in run(*arguments).stdout, "help documents resize")
    original = [[(1 + row * 41 + column * 5, 2 + row * 33 + column * 7,
                  3 + row * 29 + column * 9) for column in range(5)] for row in range(4)]
    source = root / "input with spaces.ppm"
    write_ppm(source, original)
    check(run_json("info", "--input", source) == {"width": 5, "height": 4}, "PPM input dimensions")

    for method in ("dp", "greedy", "graph"):
        for order in ("vertical-first", "horizontal-first"):
            for width, height in ((3, 2), (1, 1), (5, 4)):
                label = f"{method}_{order}_{width}x{height}"
                output = root / f"{label}.png"
                artifacts = root / f"{label}_frames" if (width, height) == (3, 2) else None
                arguments = ["resize", "--input", source, "--output", output,
                             "--width", width, "--height", height, "--method", method,
                             "--order", order]
                if artifacts is not None:
                    arguments.extend(("--artifacts", artifacts))
                result = run_json(*arguments)
                expected = verify_resize(result, original, method, width, height, order, artifacts)
                check(read_png(output) == expected, "final PNG matches replayed removals")
                check(json.loads(pathlib.Path(str(output) + ".json").read_text(encoding="utf-8")) ==
                      result, "stdout and saved resize JSON agree")
                if (width, height) == (5, 4):
                    check(read_png(output) == original, "unchanged resize preserves every RGB pixel")

    # A varying horizontal seam shifts each column differently. After the first
    # removal, further seams must still map back to the correct original pixels.
    # The DP optimum here is unique, so this also confirms the fixture actually
    # exercises a varying seam rather than relying on incidental random choices.
    levels = [[225, 211, 184, 60], [253, 43, 24, 238], [225, 127, 238, 250]]
    varying = [[(value, value, value) for value in row] for row in levels]
    varying_source = root / "varying horizontal rows.ppm"
    write_ppm(varying_source, varying)
    for method in ("dp", "greedy", "graph"):
        output = root / f"varying_horizontal_{method}.png"
        artifacts = root / f"varying_horizontal_{method}_frames"
        summary = run_json("resize", "--input", varying_source, "--output", output,
                           "--width", 2, "--height", 1, "--method", method,
                           "--order", "horizontal-first", "--artifacts", artifacts)
        first = summary["steps"][0]
        check(first["axis"] == "h" and len(set(first["columns"])) > 1,
              "focused horizontal regression selects varying source rows")
        check(first["columns"] == ([0, 1, 0, 0] if method == "greedy" else [0, 1, 1, 2]),
              "focused varying horizontal seam coordinates")
        if method != "greedy":
            check(first["cost"] == 2012, "unique varying horizontal optimum cost")
        expected = verify_resize(summary, varying, method, 2, 1,
                                 "horizontal-first", artifacts)
        check(read_png(output) == expected, "focused horizontal regression final pixels")

    for order in ("vertical-first", "horizontal-first"):
        directory = root / f"comparison_{order}"
        result = run_json("compare", "--input", source, "--output-dir", directory,
                          "--width", 3, "--height", 2, "--repeats", 2, "--order", order, "--steps")
        check(result["repeats"] == 2 and result["order"] == order, "comparison configuration")
        check((result["input_width"], result["input_height"], result["target_width"],
               result["target_height"]) == (5, 4, 3, 2), "comparison dimensions")
        check([entry["method"] for entry in result["methods"]] == ["dp", "greedy", "graph"],
              "comparison includes all three methods")
        costs = {}
        for entry in result["methods"]:
            method = entry["method"]
            expected = verify_resize(entry, original, method, 3, 2, order)
            check(read_png(directory / entry["output"]) == expected, "comparison method output")
            check_times(entry, ("median_ms", "min_ms", "max_ms"), "comparison")
            check(entry["min_ms"] <= entry["median_ms"] <= entry["max_ms"],
                  "comparison timing distribution")
            costs[method] = (entry["vertical_cost"], entry["horizontal_cost"])
            frame_summary = json.loads((directory / method / "summary.json").read_text(encoding="utf-8"))
            verify_resize(frame_summary, original, method, 3, 2, order, directory / method)
            check(read_png(directory / f"{method}_vertical.png") and
                  read_png(directory / f"{method}_horizontal.png"), "original seam overlays exist")
        check(costs["dp"] == costs["graph"], "DP/graph shared original-map seam costs agree")
        check(all(greedy >= optimal for greedy, optimal in zip(costs["greedy"], costs["dp"])),
              "greedy original-map costs are no smaller than optimum")
        check(costs["dp"] == (optimum(energy(original)), optimum(energy(transpose(original)))),
              "comparison shared-map costs independently verified")
        rows = list(csv.DictReader(io.StringIO((directory / "timings.csv").read_text(encoding="utf-8"))))
        check(len(rows) == 6, "two repetitions times three methods in CSV")
        check({(row["method"], row["repetition"]) for row in rows} ==
              {(method, repetition) for method in ("dp", "greedy", "graph")
               for repetition in ("1", "2")}, "CSV method/repetition combinations")
        for row in rows:
            check((row["width"], row["height"], row["target_width"], row["target_height"],
                   row["order"]) == ("5", "4", "3", "2", order), "CSV dimensions and order")
            check(all(math.isfinite(float(row[key])) and float(row[key]) >= 0
                      for key in ("energy_ms", "search_ms", "remove_ms", "total_ms")),
                  "CSV numeric timing fields")
        check(json.loads((directory / "comparison.json").read_text(encoding="utf-8")) == result,
              "comparison stdout and file agree")
        check(read_png(directory / "original.png") == original, "comparison original pixels")
        preview = read_png(directory / "energy.png")
        check((len(preview[0]), len(preview)) == (5, 4), "comparison energy map dimensions")

    grid = root / "counterexample.txt"
    grid.write_text("3 3\n1 2 100\n1 100 2\n100 100 1\n", encoding="ascii")
    for method, expected_cost in (("dp", 5), ("greedy", 102), ("graph", 5)):
        result = run_json("trace", "--grid", grid, "--method", method)
        check(result["method"] == method and result["cost"] == expected_cost, "trace counterexample cost")
        check(result["columns"] == ([0, 0, 0] if method == "greedy" else [1, 2, 2]),
              "trace counterexample path")
        check(result["cumulative"] == ([1, 2, 100, 2, 101, 4, 102, 102, 5]
              if method == "dp" else []), "trace cumulative-table contract")

    demo = root / "demo scenes"
    result = run_json("demo", "--output-dir", demo)
    check(result["demo_images"] == 8 and result["width"] == 160 and result["height"] == 112,
          "demo metadata")
    check(len(list(demo.glob("scene_*.png"))) == 8, "eight generated demonstration scenes")
    for path in demo.glob("scene_*.png"):
        image = read_png(path)
        check((len(image[0]), len(image)) == (160, 112), "demo PNG dimensions")
    check((demo / "greedy_counterexample.txt").read_text(encoding="ascii") ==
          grid.read_text(encoding="ascii"), "demo counterexample input")

    bad_base = ["resize", "--input", source, "--output", root / "invalid.png"]
    invalid_arguments = [
        ["unknown"], ["info"], ["info", "--input"],
        ["info", "--input", source, "--unknown", "x"],
        ["info", "--input", source, "--input", source],
        ["info", "--input", root / "missing.png"],
        ["resize", "--input", source],
        bad_base + ["--method", "unknown"],
        bad_base + ["--order", "unknown"],
        bad_base + ["--steps"],
        bad_base + ["--width", "0"],
        bad_base + ["--height", "-1"],
        bad_base + ["--width", "2.5"],
        bad_base + ["--height", "2x"],
        bad_base + ["--width", "2147483648"],
        bad_base + ["--width", "6"],
        bad_base + ["--height", "5"],
        ["compare", "--input", source, "--output-dir", root / "bad_compare", "--repeats", "0"],
        ["compare", "--input", source, "--output-dir", root / "bad_compare", "--repeats", "101"],
        ["compare", "--input", source, "--output-dir", root / "bad_compare", "--steps", "--steps"],
        ["trace", "--grid", root / "missing.txt"],
    ]
    for arguments in invalid_arguments:
        run(*arguments, success=False)
    unreadable = root / "not an image.png"
    unreadable.write_bytes(b"not a valid image")
    run("info", "--input", unreadable, success=False)
    destination_directory = root / "output_is_directory.png"
    destination_directory.mkdir()
    run("resize", "--input", source, "--output", destination_directory, success=False)
    parent_file = root / "not_a_folder"
    parent_file.write_text("occupied", encoding="ascii")
    run("resize", "--input", source, "--output", parent_file / "output.png", success=False)
    for label, text in (("truncated", "2 2\n1 2 3\n"),
                        ("trailing", "1 1\n1\nextra\n"),
                        ("negative", "1 2\n-1 2\n"),
                        ("zero_dimension", "0 1\n"),
                        ("bad_integer", "1 2\n1 x\n"),
                        ("overflow", "1 2\n9223372036854775807 1\n")):
        path = root / f"grid_{label}.txt"
        path.write_text(text, encoding="ascii")
        run("trace", "--grid", path, success=False)
    maximum = root / "grid_maximum.txt"
    maximum.write_text("1 1\n9223372036854775807\n", encoding="ascii")
    run("trace", "--grid", maximum, "--method", "graph", success=False)

    # CJK characters ensure this tests UTF-8 argument handling as well as spaces.
    unicode_directory = root / "画像 café with spaces"
    unicode_directory.mkdir()
    unicode_source = unicode_directory / "入力 image.ppm"
    unicode_output = unicode_directory / "出力 image.png"
    write_ppm(unicode_source, original)
    check(run_json("info", "--input", unicode_source) == {"width": 5, "height": 4},
          "Unicode input path")
    result = run_json("resize", "--input", unicode_source, "--output", unicode_output,
                      "--width", 5, "--height", 4)
    verify_resize(result, original, "dp", 5, 4, "vertical-first")
    check(read_png(unicode_output) == original, "Unicode output path and unchanged identity")
    test_codecs(root)
    test_destination_safety(root, source, original)


if __name__ == "__main__":
    try:
        build = pathlib.Path(__file__).resolve().parents[1] / "build"
        build.mkdir(exist_ok=True)
        with tempfile.TemporaryDirectory(prefix="cli integration ", dir=build) as temporary:
            main(pathlib.Path(temporary))
        print(f"PASS: {CHECKS} integration assertions; {INVOCATIONS} CLI invocations; "
              f"{RESIZES} resize result sequences replayed independently.")
        for skipped in SKIPPED:
            print(f"SKIP: {skipped}")
    except (AssertionError, OSError, subprocess.SubprocessError, ValueError,
            KeyError, TypeError, zlib.error) as error:
        print(f"FAIL after {CHECKS} integration assertions: {error}", file=sys.stderr)
        sys.exit(1)
