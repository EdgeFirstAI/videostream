import pytest
import numpy as np
from videostream import Frame
from pathlib import Path
from random import randint
from warnings import warn


def test_frame():
    """
        Basic test demonstrating we can create two frames which share memory.
        In this case frame 1 will allocate from the default space: dma_heap if
        available, otherwise shared memory.  We then create frame 2 using the
        handle from frame 1.  We demonstrate a few examples where the memory
        is shared, including zero-copy memoryviews with Numpy.
    """
    frame = Frame(640, 480, fourcc='RGB3')
    assert frame.width == 640
    assert frame.height == 480
    assert frame.fourcc == 'RGB3'
    assert frame.path is None

    with pytest.raises(Exception):
        frame.handle

    frame.alloc()
    assert frame.size == 640 * 480 * 3

    v1 = frame.map()
    assert len(v1) == 640 * 480 * 3

    a1 = np.frombuffer(v1, dtype=np.uint8)
    a1[:] = np.random.randint(0, 255, len(v1), dtype=np.uint8)
    assert (a1 == v1).all()
    frame2 = Frame(640, 480, fourcc='RGB3')
    frame2.attach(frame.handle)
    v2 = frame2.map()
    assert (a1 == v2).all()
    for i in range(len(v2)):
        v2[i] = randint(0, 255)
    assert v1 == v2

    try:
        assert frame.paddr != 0
    except Exception as err:
        warn('Physical Address: %s' % err, UserWarning)

    frame2.unmap()
    frame2.unalloc()

    frame.unmap()
    frame.unalloc()

    with pytest.raises(Exception):
        frame.handle


def test_attach_file():
    """
        This test demonstrates how we can create a frame in our filesystem then
        associated the file descriptor to a videostream frame to use normally.
    """
    frame = Frame(640, 480, fourcc='RGB3')
    expect = np.random.randint(
        0, 255, frame.stride * frame.height, dtype=np.uint8)
    # Note the mode 'w+b' is important here as without the ability to  write
    # 'w' AND read 'w+' the mapping will fail.  The 'b' represents binary.
    with open('/tmp/test_frame.vsl', 'w+b') as f:
        f.truncate(frame.stride * frame.height)
        frame.attach(f.fileno())
    v = frame.map()
    v[:] = expect
    del v
    frame.unmap()
    out = np.fromfile('/tmp/test_frame.vsl', dtype=np.uint8)
    assert (expect == out).all()


def test_bad_attach():
    """
        Tests various invalid file descriptor attachments to ensure they throw
        errors and don't lead to further issues, such as crashes.
    """
    frame = Frame(640, 480, fourcc='RGB3')
    with pytest.raises(Exception):
        frame.attach(-1)

    # Assuming fd are allocated sequentially, currently the case, and picking a
    # large enough fd that it shouldn't be open currently.
    with pytest.raises(Exception):
        frame.attach(9000)

    # fd 0 (stdin) is now correctly rejected by attach to prevent fd corruption
    with pytest.raises(Exception):
        frame.attach(0)


@pytest.mark.parametrize("fourcc,stride", [
    ('RGB3', 3), ('RGBX', 4), ('RGBA', 4),
    ('BGR3', 3), ('BGRX', 4), ('BGRA', 4),
    ('YUYV', 2), ('UYVY', 2), ('YVYU', 2), ('VYUY', 2), ('YUY2', 2),
    ('I420', 1.5), ('NV12', 1.5), ('YV12', 1.5), ('NV12', 1.5),
])
def test_fourcc(fourcc, stride):
    """
        Test we can create frames for some known fourcc codes and that others
        will raise exceptions for the unsupported formats.  We also test some
        assumptions about frame sizes and strides for various fourcc codes.
    """
    frame = Frame(4, 3, fourcc)
    assert frame.stride == stride * 4
    frame.alloc()
    assert frame.size == stride * 4 * 3


@pytest.mark.parametrize("fourcc", ['GRB', '    ', '', None])
def test_bad_fourcc(fourcc):
    """
        Test for some expected errors when using invalid or unsupported fourcc.
    """
    with pytest.raises(Exception):
        Frame(4, 3, fourcc)


def test_nodma():
    """
        Test verifies that attempting to allocate from dma_heap fails when it
        is not available.  We test a dma_heap which is expected to never exist.
    """
    frame = Frame(640, 480, fourcc='RGB3')
    with pytest.raises(Exception):
        frame.alloc('/dev/dma_heap/invalid')


def test_invalid_shm_name():
    """
        When not using dma_heap, VSL uses POSIX Shared Memory for buffers.  It
        requires names to start with a / but not have additional / in the name.

        These are not actual filesystem objects, so giving a filesystem path
        such as /tmp/invalid should raise an error.
    """
    frame = Frame(640, 480, fourcc='RGB3')
    with pytest.raises(Exception):
        frame.alloc('/tmp/invalid')


def test_shm_fill_benchmark(benchmark):
    """
        The Shared Memory fill benchmark.
    """
    frame = Frame(640, 480, fourcc='RGB3')
    frame.alloc('/VSL_BENCH_SHM')
    v1 = np.frombuffer(frame.map(), dtype=np.uint8).reshape(480, 640, 3)
    assert v1.size == 640 * 480 * 3

    def fill():
        v1[:] = np.ones((480, 640, 3), dtype=np.uint8)
    benchmark(fill)


def test_dma_fill_benchmark(benchmark):
    """
        The DMA fill benchmark.
    """
    dma_heap_path = Path('/dev/dma_heap')
    if not dma_heap_path.exists():
        pytest.skip(
            "DMA heap not available - test requires /dev/dma_heap "
            "(embedded target only)"
        )

    # Verify DMA heap is actually usable by attempting allocation
    try:
        test_frame = Frame(64, 64, fourcc='RGB3')
        test_frame.alloc()
        if not test_frame.path.startswith('/dev/dma_heap'):
            pytest.skip(
                "DMA heap exists but allocation fell back to shared "
                "memory - check /dev/dma_heap permissions (may need "
                "video group membership)"
            )
        del test_frame
    except Exception as e:
        pytest.skip(
            f"DMA heap exists but cannot allocate: {e}"
        )

    frame = Frame(640, 480, fourcc='RGB3')
    frame.alloc()
    if not frame.path.startswith('/dev/dma_heap'):
        pytest.skip(
            f"Frame allocation unexpectedly fell back to "
            f"{frame.path} - DMA heap may be out of memory"
        )
    v1 = np.frombuffer(frame.map(), dtype=np.uint8).reshape(480, 640, 3)
    assert v1.size == 640 * 480 * 3

    def fill():
        v1[:] = np.ones((480, 640, 3), dtype=np.uint8)
    benchmark(fill)


def test_dma_fill_remap_benchmark(benchmark):
    """
        The DMA fill benchmark, but with added unmap/map in the loop which
        will trigger cache flush/refresh.  This benchmark will be slower
        because of the cache synchronization and represents the cost of DMA
        buffers when sharing with hardware.
    """
    dma_heap_path = Path('/dev/dma_heap')
    if not dma_heap_path.exists():
        pytest.skip(
            "DMA heap not available - test requires /dev/dma_heap "
            "(embedded target only)"
        )

    # Verify DMA heap is actually usable by attempting allocation
    try:
        test_frame = Frame(64, 64, fourcc='RGB3')
        test_frame.alloc()
        if not test_frame.path.startswith('/dev/dma_heap'):
            pytest.skip(
                "DMA heap exists but allocation fell back to shared "
                "memory - check /dev/dma_heap permissions (may need "
                "video group membership)"
            )
        del test_frame
    except Exception as e:
        pytest.skip(
            f"DMA heap exists but cannot allocate: {e}"
        )

    frame = Frame(640, 480, fourcc='RGB3')
    frame.alloc()
    if not frame.path.startswith('/dev/dma_heap'):
        pytest.skip(
            f"Frame allocation unexpectedly fell back to "
            f"{frame.path} - DMA heap may be out of memory"
        )

    def fill():
        v1 = np.frombuffer(frame.map(), dtype=np.uint8).reshape(480, 640, 3)
        v1[:] = np.ones((480, 640, 3), dtype=np.uint8)
        del v1  # Explicitly remove v1 before frame.unmap()
        frame.unmap()
    benchmark(fill)


def test_dma_fill_sync_benchmark(benchmark):
    """
        The DMA fill benchmark, but with added sync in the loop to provide
        more fine-trained synchronization than forcing map/unmap.

        This benchmark will be slower because of the cache synchronization
        and represents the cost of DMA buffers when sharing with hardware.
    """
    dma_heap_path = Path('/dev/dma_heap')
    if not dma_heap_path.exists():
        pytest.skip(
            "DMA heap not available - test requires /dev/dma_heap "
            "(embedded target only)"
        )

    # Verify DMA heap is actually usable by attempting allocation
    try:
        test_frame = Frame(64, 64, fourcc='RGB3')
        test_frame.alloc()
        if not test_frame.path.startswith('/dev/dma_heap'):
            pytest.skip(
                "DMA heap exists but allocation fell back to shared "
                "memory - check /dev/dma_heap permissions (may need "
                "video group membership)"
            )
        del test_frame
    except Exception as e:
        pytest.skip(
            f"DMA heap exists but cannot allocate: {e}"
        )

    frame = Frame(640, 480, fourcc='RGB3')
    frame.alloc()
    if not frame.path.startswith('/dev/dma_heap'):
        pytest.skip(
            f"Frame allocation unexpectedly fell back to "
            f"{frame.path} - DMA heap may be out of memory"
        )
    v1 = np.frombuffer(frame.map(), dtype=np.uint8).reshape(480, 640, 3)
    frame.sync(False, Frame.Sync.RW)  # Disable sync after map

    def fill():
        frame.sync(True, Frame.Sync.WRONLY)
        v1[:] = np.ones((480, 640, 3), dtype=np.uint8)
        frame.sync(False, Frame.Sync.WRONLY)
    benchmark(fill)


def test_numpy_fill_benchmark(benchmark):
    """
        The reference benchmark using standard Numpy buffers.  This benchmark
        should not be measurably faster or slower than the shared memory or
        dma_heap benchmarks.
    """
    v1 = np.zeros((480, 640, 3), dtype=np.uint8)
    assert v1.size == 640 * 480 * 3

    def fill():
        v1[:] = np.ones((480, 640, 3), dtype=np.uint8)
    benchmark(fill)
