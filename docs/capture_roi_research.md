# Capture ROI investigation

Status: research notes and a proposed optimization. The current source does not implement ROI-sized native capture or explicit SSE2 scanning.

## What the application captures

Replica thumbnails are already displayed through DWM thumbnail handles. The pixel capture path is used by color statistics and explicit freeze capture; statistics batches capture a source once and scan the configured region(s). The DWM thumbnail API describes a live thumbnail relationship, while `PrintWindow` synchronously asks the target window to render into a supplied DC.

## Current implementation

[`NativeGateway::captureWindowStatistics`](../cpp/src/platform/native_gateway.cpp) calls `eve_viola_capture_frame` when both frame capture and release functions are available; otherwise it uses the legacy pixel-buffer interface. It captures once per statistics batch, scans each distinct configured region once, and distributes those results to replicas requesting the same region. There is no `eve_viola_capture_frame_region` export in the public header.

[`window_capture_frame.cpp`](../native/src/window_capture_frame.cpp) allocates or reuses a full-window DIB and calls `PrintWindow`. If rendering fails, it attempts `BitBlt` from the source window DC. The returned frame is a client-area view with the full-window row stride; retained frames keep their surface alive until release. This path does not copy an ROI from the desktop or check foreground visibility and z-order.

[`pixel_statistics_calculator.cpp`](../cpp/src/features/replica/pixel_statistics_calculator.cpp) clips the requested region and classifies pixels in a scalar loop. Restricting the statistics region reduces scanning work, but does not reduce the DIB allocation or the window render area. No explicit SSE2 implementation is present.

## Proposed ROI path

A possible optimization is to union finite statistics regions and copy that rectangle from the desktop into an ROI-sized DIB only when the source region is fully visible and unobscured. Background, off-screen, minimized, layered, or overlapped sources would need the full-window path. The frame contract would also need to report its client-space origin so the gateway can map statistics regions correctly.

This is a proposal, with no recorded EVE correctness or performance measurements. Desktop pixels may differ from `PrintWindow` output under display color transforms. Reduced copied bytes alone would not establish a performance gain; eligibility checks and desktop-copy cost must also be measured.

## Evidence and limits

- Chandra et al., “Understanding Screen Contents for Building a High Performance, Real Time Screen Sharing System,” ACM Multimedia 2012, DOI [10.1145/2393347.2393404](https://doi.org/10.1145/2393347.2393404), [paper PDF](https://projet.liris.cnrs.fr/imagine/pub/proceedings/ACM-MULTIMEDIA-2012/mm/p389.pdf). Its observations about sparse screen updates motivate measuring ROI work, but do not establish that EVE content is sparse or that `PrintWindow` can skip source rendering.
- Hsu et al., “Screencast Dissected: Performance Measurements and Design Considerations,” ACM MMSys 2015, DOI [10.1145/2713168.2713176](https://doi.org/10.1145/2713168.2713176), [author PDF](https://people.cs.nycu.edu.tw/~chuang/pubs/pdf/2015mmsys.pdf). It motivates measuring across resolution, frame rate, and content; its codec findings do not directly apply to local thumbnail statistics.
- Microsoft [`PrintWindow`](https://learn.microsoft.com/windows/win32/api/winuser/nf-winuser-printwindow) documents a synchronous request for the target application to render into a DC. Microsoft [`CreateDIBSection`](https://learn.microsoft.com/windows/win32/api/wingdi/nf-wingdi-createdibsection) documents direct pixel access and the need to synchronize GDI writes with `GdiFlush`; the existing capture code flushes before exposing the pixels to CPU readers.
- Microsoft [DWM thumbnails](https://learn.microsoft.com/en-us/windows/win32/dwm/thumbnail-ovw) describes the live thumbnail mechanism already used for replica display; the proposed ROI optimization concerns statistics capture.
- The [WebRTC Windows GDI capturer](https://webrtc.googlesource.com/src/+/8e2442f1a1e786f1061ff024e9d331f92865e659/modules/desktop_capture/win/window_capturer_win_gdi.cc) documents a visibility-dependent choice between `PrintWindow` for occluded windows and screen capture/cropping for visible windows. It is a reference for the proposed visibility split; this application's current capture path does not perform that split or a z-order overlap check.

## Verification still needed

For the current implementation, [`scripts/profile_performance.ps1`](../scripts/profile_performance.ps1) accepts `-ConfigDirectory <cache-directory>` and `-CompareLegacy`. The latter runs the probe with and without `--legacy-capture`, comparing the legacy copied pixel buffer with the retained DIB frame. Both paths use full-window `PrintWindow` capture with a `BitBlt` fallback; this comparison does not measure an ROI backend.

If an ROI path is implemented, compare its region statistics against the full-frame path with the target foreground, background, partially covered, and fully occluded. Include representative client and ROI sizes, and inspect `native.capture_total`, `native.render`, `native.readback`, and `statistics.scan`. Add native coverage for client-space offsets, visibility checks, fallback behavior, and frame ownership; the existing full-frame tests do not cover a proposed desktop ROI path. Do not infer an EVE performance percentage from reduced bytes alone.
