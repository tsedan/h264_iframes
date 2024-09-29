import av
import lzma
import pathlib
import subprocess
import ctypes
import colorama
from typing import Callable
from PIL.Image import Image


def encode(
    infile: pathlib.Path,
    interval: int,
    encode_image: Callable[[Image], bytes],
    decode_image: Callable[[bytes], Image],
):
    pretty("loading libraries")
    h264 = load_lib()

    outfile = desire(infile.with_suffix(".lrc"))
    pretty("encoding " + infile.name + " to " + outfile.name)

    temp_path = infile.with_stem(infile.stem + "_enc")
    temp_vid_path = desire(temp_path)

    reader = av.open(infile, "r")
    writer = av.open(temp_vid_path, "w")

    in_stream = reader.streams.video[0]
    codec_name = in_stream.codec_context.name
    fps = in_stream.codec_context.rate

    out_stream = writer.add_stream(codec_name, fps)
    out_stream.width = in_stream.codec_context.width
    out_stream.height = in_stream.codec_context.height
    out_stream.pix_fmt = in_stream.codec_context.pix_fmt

    num_frames = 0

    frame_encodings = []

    for frame in reader.decode(video=0):
        img = frame.to_image()

        if num_frames % interval == 0:
            encoded = lzma.compress(encode_image(img))
            frame_encodings.append(encoded)
            img = decode_image(lzma.decompress(encoded))

        res = av.VideoFrame.from_image(img)
        for packet in out_stream.encode(res):
            writer.mux(packet)

        num_frames += 1

    for packet in out_stream.encode():
        writer.mux(packet)

    writer.close()
    reader.close()

    pretty("now converting to h264")

    temp_264_path = desire(temp_path.with_suffix(".264"))
    ffmpeg(temp_vid_path, temp_264_path, interval)

    pretty("now substituting compressed frame data")

    hstream = h264.hstream_new(
        str(temp_264_path).encode(),
        str(outfile).encode(),
    )

    total, frames, iframes = 0, 0, 0
    while h264.hstream_next(hstream) != -1:
        frame_type = h264.hstream_type(hstream)
        if frame_type == -1:
            panic("Error on h264 read")

        if frame_type == 5:
            payload = frame_encodings[iframes]
            size = len(payload)

            data = (ctypes.c_char * size)(*payload)

            wrote = h264.hstream_set(hstream, data, size)
            if wrote == -1 or wrote != size:
                panic("Error on h264 set")

            iframes += 1

        if 1 <= frame_type <= 5:
            frames += 1

        total += 1

    h264.hstream_del(hstream)

    print("Total NAL Units:", total)
    print("Number of Frames:", frames)
    print("Number of I-Frames:", iframes)

    pretty("now removing temporary files")

    temp_vid_path.unlink()
    # temp_264_path.unlink() # don't remove this so we can see compression ratio

    pretty("encoding completed to " + str(outfile))


def decode(
    infile: pathlib.Path,
    decode_image: Callable[[bytes], Image],
):
    pretty("loading libraries")
    h264 = load_lib()

    outfile = desire(infile.with_suffix(".mp4"))
    pretty("decoding " + infile.name + " to " + outfile.name)

    dec_path = infile.with_stem(infile.stem + "_dec")
    inter_path = desire(dec_path.with_suffix(".264"))

    hstream = h264.hstream_new(
        str(infile).encode(),
        str(inter_path).encode(),
    )

    total, frames, iframes = 0, 0, 0
    while h264.hstream_next(hstream) != -1:
        frame_type = h264.hstream_type(hstream)
        if frame_type == -1:
            panic("Error on h264 read")

        if frame_type == 5:
            iframes += 1

            size = h264.hstream_size(hstream)
            if size == -1:
                panic("Error on h264 size")

            payload = (ctypes.c_char * size)()
            read = h264.hstream_get(hstream, payload, size)
            if read == -1 or read != size:
                panic("Error on h264 get")

            img = decode_image(lzma.decompress(b"".join(payload)))

            vid_path = desire(dec_path.with_suffix(".mp4"))

            reader = av.open(infile.with_suffix(".mp4"), "r")
            writer = av.open(vid_path, "w")

            in_stream = reader.streams.video[0]
            codec_name = in_stream.codec_context.name
            fps = in_stream.codec_context.rate

            out_stream = writer.add_stream(codec_name, fps)
            out_stream.width = in_stream.codec_context.width
            out_stream.height = in_stream.codec_context.height
            out_stream.pix_fmt = in_stream.codec_context.pix_fmt

            res = av.VideoFrame.from_image(img)
            for packet in out_stream.encode(res):
                writer.mux(packet)

            for packet in out_stream.encode():
                writer.mux(packet)

            writer.close()
            reader.close()

            h264_path = desire(dec_path.with_suffix(".264"))
            ffmpeg(vid_path, h264_path)

            vid_path.unlink()

            tstream = h264.hstream_new(
                str(h264_path).encode(),
                None,
            )

            while h264.hstream_next(tstream) != -1:
                frame_type = h264.hstream_type(tstream)
                if frame_type == -1:
                    panic("Error on h264 read")

                if frame_type == 5:
                    size = h264.hstream_size(tstream)
                    if size == -1:
                        panic("Error on h264 size")

                    payload = (ctypes.c_char * size)()
                    read = h264.hstream_get(tstream, payload, size)
                    if read == -1 or read != size:
                        panic("Error on h264 get")

                    break

            h264.hstream_del(tstream)

            h264_path.unlink()

            wrote = h264.hstream_set(hstream, payload, size)
            if wrote == -1 or wrote != size:
                panic("Error on h264 set")

        if 1 <= frame_type <= 5:
            frames += 1

        total += 1

    h264.hstream_del(hstream)

    print("Total NAL Units:", total)
    print("Number of Frames:", frames)
    print("Number of I-Frames:", iframes)

    pretty("creating final video file")

    ffmpeg(inter_path, outfile)

    pretty("now removing temporary files")

    inter_path.unlink()

    pretty("decoding completed to " + str(outfile))


def load_lib():
    libpath = pathlib.Path(__file__).parent / "bin" / "libiframes.so"
    h264 = ctypes.CDLL(str(libpath.resolve()))

    h264.hstream_new.argtypes = [ctypes.c_char_p, ctypes.c_char_p]
    h264.hstream_new.restype = ctypes.c_void_p

    h264.hstream_del.argtypes = [ctypes.c_void_p]
    h264.hstream_del.restype = ctypes.c_int

    h264.hstream_next.argtypes = [ctypes.c_void_p]
    h264.hstream_next.restype = ctypes.c_int

    h264.hstream_type.argtypes = [ctypes.c_void_p]
    h264.hstream_type.restype = ctypes.c_int

    h264.hstream_size.argtypes = [ctypes.c_void_p]
    h264.hstream_size.restype = ctypes.c_ssize_t

    h264.hstream_get.argtypes = [ctypes.c_void_p, ctypes.c_char_p, ctypes.c_size_t]
    h264.hstream_get.restype = ctypes.c_ssize_t

    h264.hstream_set.argtypes = [ctypes.c_void_p, ctypes.c_char_p, ctypes.c_size_t]
    h264.hstream_set.restype = ctypes.c_ssize_t

    return h264


def ffmpeg(infile: pathlib.Path, outfile: pathlib.Path, interval: int | None = None):
    base = ["ffmpeg", "-y", "-i", str(infile)]
    if interval is not None:
        base += [
            "-c:v",
            "libx264",
            "-x264opts",
            f"keyint={interval}:min-keyint={interval}:no-scenecut",
        ]
    subprocess.run(base + [str(outfile)])


def desire(path: pathlib.Path) -> pathlib.Path:
    """You want a path, but want to make sure the file doesn't already exist"""
    counter = 1
    stem = path.stem
    while path.exists():
        counter += 1
        path = path.with_stem(f"{stem}_{counter}")
    return path


def pretty(msg: str):
    print(colorama.Fore.GREEN + msg + colorama.Style.RESET_ALL)


def panic(msg: str):
    print(colorama.Fore.RED + msg + colorama.Style.RESET_ALL)
    exit(1)
