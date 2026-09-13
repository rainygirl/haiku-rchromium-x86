import sys, shutil
P = "/boot/home/rchromium-chromium87-fast/chromium/content/renderer/render_frame_impl.cc"
# restore the pristine (pre-0086) file, then apply the ignore-based fix
shutil.copy2("/boot/home/render_frame_impl.cc.prepatch86", P)
s = open(P).read()
old = '''#if !defined(OS_ANDROID)
      // This check is not enabled on Android, since it seems like it's much
      // easier to trigger data races there.
      CHECK(!in_frame_tree_);
#endif  // !defined(OS_ANDROID)
      break;'''
new = '''#if defined(OS_HAIKU)
      // The race this CHECK guards (crbug.com/838348: the browser cancels a
      // speculative main-frame navigation just as the renderer swaps it in)
      // is easy to hit on the 2-core Atom this port runs on -- every real
      // navigation, including the first, tripped it. Android disables the
      // CHECK and falls through to Detach(), but on Haiku that Detach() of an
      // already-swapped-in main frame left the bad pointers the comment above
      // warns of and the renderer took SEGV_ACCERR on load. So take the same
      // escape the kSpeculativeMainFrameForShutdown case above uses when the
      // frame is already in the tree: ignore the delete. The browser is left
      // believing the RenderView has a remote main frame when it does not --
      // an inconsistency, but a survivable one, and far better than either a
      // crash or a use-after-free.
      if (in_frame_tree_)
        return;
#else
      // This check is not enabled on Android, since it seems like it's much
      // easier to trigger data races there.
      CHECK(!in_frame_tree_);
#endif  // defined(OS_HAIKU)
      break;'''
if s.count(old) != 1:
    sys.exit("anchor found %d times" % s.count(old))
open(P, "w").write(s.replace(old, new))
print("render_frame_impl.cc re-patched (ignore instead of Detach)")
