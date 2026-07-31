// CP-B20: compiled C# helper used by tools/visual_audit/diff_scorer.ps1.
//
// Why this exists as a .cs file rather than a here-string in the .ps1:
// PowerShell 7 splits System.Drawing into System.Drawing.Common +
// System.Drawing.Primitives, and Add-Type -ReferencedAssemblies under
// PS 7 chokes on the missing transitive references (System.Private.Windows.
// GdiPlus, System.Private.Windows.Core, etc.) that `new Bitmap(path)`
// pulls in. Compiling on the command line once at the start of the
// scorer, then loading the resulting DLL with Add-Type -Path, sidesteps
// the reference-resolution entirely. The scorer invokes:
//   & csc.exe -nologo -target:library NfuiDiffHelper.cs -out:$env:TEMP\NfuiDiffHelper.dll
//   Add-Type -Path $env:TEMP\NfuiDiffHelper.dll
// and then calls [NfuiDiffHelper]::DiffPngs(...) per capture.

using System;
using System.Drawing;
using System.Drawing.Imaging;
using System.Runtime.InteropServices;

public static class NfuiDiffHelper
{
    public static object DiffPngs(string auditPath, string basePath, int pixelThreshold)
    {
        using (var auditImg = new Bitmap(auditPath))
        using (var baseImg = new Bitmap(basePath))
        {
            if (auditImg.Width != baseImg.Width || auditImg.Height != baseImg.Height)
            {
                return new {
                    Width = auditImg.Width, Height = auditImg.Height,
                    BaseWidth = baseImg.Width, BaseHeight = baseImg.Height,
                    SizeMismatch = true, Max = 255, Mean = 255, DiffPctX100 = 10000
                };
            }
            int w = auditImg.Width, h = auditImg.Height;
            long totalPixels = (long)w * h;
            Bitmap a32 = auditImg.PixelFormat == PixelFormat.Format32bppArgb
                ? auditImg
                : auditImg.Clone(new Rectangle(0, 0, w, h), PixelFormat.Format32bppArgb);
            Bitmap b32 = baseImg.PixelFormat == PixelFormat.Format32bppArgb
                ? baseImg
                : baseImg.Clone(new Rectangle(0, 0, w, h), PixelFormat.Format32bppArgb);
            bool disposeA = (a32 != auditImg);
            bool disposeB = (b32 != baseImg);
            try
            {
                var rect = new Rectangle(0, 0, w, h);
                var aData = a32.LockBits(rect, ImageLockMode.ReadOnly, PixelFormat.Format32bppArgb);
                var bData = b32.LockBits(rect, ImageLockMode.ReadOnly, PixelFormat.Format32bppArgb);
                int bufSize = aData.Stride * h;
                byte[] aBuf = new byte[bufSize];
                byte[] bBuf = new byte[bufSize];
                try
                {
                    Marshal.Copy(aData.Scan0, aBuf, 0, bufSize);
                    Marshal.Copy(bData.Scan0, bBuf, 0, bufSize);
                }
                finally
                {
                    a32.UnlockBits(aData);
                    b32.UnlockBits(bData);
                }
                int maxDiff = 0;
                long sumDiff = 0;
                long diffPixels = 0;
                // 32bpp ARGB layout is B, G, R, A. Skip alpha (every 4th byte).
                for (int i = 0; i < bufSize; i += 4)
                {
                    int db = Math.Abs(aBuf[i]     - bBuf[i]);
                    int dg = Math.Abs(aBuf[i + 1] - bBuf[i + 1]);
                    int dr = Math.Abs(aBuf[i + 2] - bBuf[i + 2]);
                    int localMax = dr > dg ? dr : dg;
                    if (db > localMax) localMax = db;
                    sumDiff += (dr + dg + db);
                    if (localMax > maxDiff) maxDiff = localMax;
                    if (localMax > pixelThreshold) diffPixels++;
                }
                int meanDiff = (int)(sumDiff / (totalPixels * 3));
                long diffPctX100 = (diffPixels * 10000L) / totalPixels;
                return new {
                    Width = w, Height = h,
                    BaseWidth = w, BaseHeight = h,
                    SizeMismatch = false, Max = maxDiff, Mean = meanDiff, DiffPctX100 = diffPctX100
                };
            }
            finally
            {
                if (disposeA) a32.Dispose();
                if (disposeB) b32.Dispose();
            }
        }
    }
}
