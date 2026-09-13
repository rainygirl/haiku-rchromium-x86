import sys, shutil

P = ("/boot/home/rchromium-chromium87-fast/chromium/base/allocator/"
     "partition_allocator/page_allocator_internals_posix.h")
shutil.copy2(P, "/boot/home/page_allocator_internals_posix.h.prepatch82")
s = open(P).read()

old = """  int access_flag = GetAccessFlags(accessibility);
  int map_flags = MAP_ANONYMOUS | MAP_PRIVATE;
"""
new = """  int access_flag = GetAccessFlags(accessibility);
  int map_flags = MAP_ANONYMOUS | MAP_PRIVATE;

#if defined(OS_HAIKU)
  // Haiku does not overcommit, so a PROT_NONE "reservation" is charged in
  // full unless MAP_NORESERVE says otherwise.
  //
  // PartitionAlloc reserves enormous ranges this way and opens them a piece
  // at a time with mprotect: V8 alone takes a 4 GB pointer-compression cage
  // and a 128 MB CodeRange per isolate. On Linux that costs nothing. On a
  // machine with 2 GB of RAM and no overcommit it fails, and V8 is not
  // written to survive the failure -- it dies later and somewhere else. What
  // that looked like here: the browser worked in the first half hour after a
  // reboot and then stopped rendering entirely as commit charge accumulated,
  // with Blink resolving zero styles and the process taking
  // "signal 4 ILL_PRVOPC" inside Builtins_MemMove -- executing a CodeRange
  // that was never really there.
  //
  // MAP_NORESERVE is supported (headers/posix/sys/mman.h; the kernel turns it
  // into an overcommitting area), so only the reservation becomes lazy. Real
  // pages are still charged when first written, as on Linux.
  if (access_flag == PROT_NONE)
    map_flags |= MAP_NORESERVE;
#endif
"""
if s.count(old) != 1:
    sys.exit("anchor found %d times" % s.count(old))
open(P, "w").write(s.replace(old, new))
print("patched")
