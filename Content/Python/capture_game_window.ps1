# Capture Unreal -game GAME VIEWPORT (not log console) to media/post-baseline-0N.png
param(
  [string]$Media = "C:\Users\whdtn\AppData\Local\Cursor\AgentStores\cursor_agent_stores\bc-9b1d847c-0b2a-4c36-9619-bba89962dfc5\files\media",
  [int]$WaitBootSec = 120
)

$ErrorActionPreference = "Stop"
Add-Type -AssemblyName System.Drawing
Add-Type @"
using System;
using System.Text;
using System.Collections.Generic;
using System.Runtime.InteropServices;
public class WinCap2 {
  public delegate bool EnumWindowsProc(IntPtr hWnd, IntPtr lParam);
  [DllImport("user32.dll")] public static extern bool EnumWindows(EnumWindowsProc lpEnumFunc, IntPtr lParam);
  [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr hWnd, out uint lpdwProcessId);
  [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr hWnd);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetWindowText(IntPtr hWnd, StringBuilder lpString, int nMaxCount);
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr hWnd, out RECT lpRect);
  [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr hWnd, IntPtr hdcBlt, int nFlags);
  [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr hWnd);
  [StructLayout(LayoutKind.Sequential)] public struct RECT { public int Left, Top, Right, Bottom; }
  public static List<IntPtr> FindProcessWindows(uint pid) {
    var list = new List<IntPtr>();
    EnumWindows((h, l) => {
      uint wpid; GetWindowThreadProcessId(h, out wpid);
      if (wpid == pid && IsWindowVisible(h)) list.Add(h);
      return true;
    }, IntPtr.Zero);
    return list;
  }
  public static string Title(IntPtr h) {
    var sb = new StringBuilder(512);
    GetWindowText(h, sb, sb.Capacity);
    return sb.ToString();
  }
}
"@

New-Item -ItemType Directory -Force -Path $Media | Out-Null
Get-Process UnrealEditor*, LiveCoding* -ErrorAction SilentlyContinue | ForEach-Object { Stop-Process -Id $_.Id -Force -ErrorAction SilentlyContinue }
Start-Sleep -Seconds 3

$ue = "D:\UnrealEngine\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe"
$proj = "D:\UnrealGames\RisingSlime\RisingSlime.uproject"
# -game GPU; suppress log window as primary by not attaching console as main
$argList = @(
  $proj,
  "/Game/Maps/Hunt",
  "-game",
  "-windowed",
  "-ResX=1600",
  "-ResY=900",
  "-FORCELOGFLUSH"
)
Write-Host "Launching GPU -game..."
$proc = Start-Process -FilePath $ue -ArgumentList $argList -PassThru

function Pick-GameHwnd([uint32]$ProcessId) {
  $best = [IntPtr]::Zero
  $bestArea = 0
  $bestTitle = ""
  foreach ($h in [WinCap2]::FindProcessWindows($ProcessId)) {
    $title = [WinCap2]::Title($h)
    $rect = New-Object WinCap2+RECT
    if (-not [WinCap2]::GetWindowRect($h, [ref]$rect)) { continue }
    $w = $rect.Right - $rect.Left
    $hgt = $rect.Bottom - $rect.Top
    $area = $w * $hgt
    # Skip tiny / console-like titles
    if ($w -lt 800 -or $hgt -lt 500) { continue }
    if ($title -match "cmd\.exe|Console|Log" -and $title -notmatch "Rising|Hunt") { continue }
    # Prefer game-ish titles and larger windows
    $score = $area
    if ($title -match "RisingSlime|Hunt|UnrealEditor") { $score += 5000000 }
    if ($title -eq "" -or $title -match "UnrealEditor\.exe$") { $score -= 1000000 }
    if ($score -gt $bestArea) {
      $bestArea = $score
      $best = $h
      $bestTitle = $title
    }
  }
  return @{ Hwnd = $best; Title = $bestTitle; Area = $bestArea }
}

$deadline = (Get-Date).AddSeconds($WaitBootSec)
$pick = $null
while ((Get-Date) -lt $deadline) {
  Start-Sleep -Seconds 3
  if (-not (Get-Process -Id $proc.Id -ErrorAction SilentlyContinue)) { throw "Editor exited early" }
  $pick = Pick-GameHwnd ([uint32]$proc.Id)
  Write-Host ("probe hwnd={0} title='{1}' area={2}" -f $pick.Hwnd, $pick.Title, $pick.Area)
  if ($pick.Hwnd -ne [IntPtr]::Zero -and $pick.Area -ge (800 * 500)) {
    # Extra settle after window appears
    Start-Sleep -Seconds 12
    $pick2 = Pick-GameHwnd ([uint32]$proc.Id)
    if ($pick2.Hwnd -ne [IntPtr]::Zero) { $pick = $pick2 }
    break
  }
}

if ($null -eq $pick -or $pick.Hwnd -eq [IntPtr]::Zero) { throw "No suitable game window found" }
Write-Host ("CAPTURE hwnd={0} title='{1}'" -f $pick.Hwnd, $pick.Title)

function Capture-Hwnd([IntPtr]$h, [string]$outPath) {
  $rect = New-Object WinCap2+RECT
  [WinCap2]::GetWindowRect($h, [ref]$rect) | Out-Null
  $w = [Math]::Max(1, $rect.Right - $rect.Left)
  $hgt = [Math]::Max(1, $rect.Bottom - $rect.Top)
  $bmp = New-Object System.Drawing.Bitmap $w, $hgt
  $g = [System.Drawing.Graphics]::FromImage($bmp)
  $hdc = $g.GetHdc()
  $ok = [WinCap2]::PrintWindow($h, $hdc, 2)
  $g.ReleaseHdc($hdc)
  if (-not $ok) {
    $g.CopyFromScreen($rect.Left, $rect.Top, 0, 0, (New-Object System.Drawing.Size($w, $hgt)))
  }
  $g.Dispose()
  $bmp.Save($outPath, [System.Drawing.Imaging.ImageFormat]::Png)
  $len = (Get-Item $outPath).Length
  # Detect mostly-black (failed capture)
  $sample = $bmp.GetPixel([int]($w/2), [int]($hgt/2))
  $bmp.Dispose()
  Write-Host ("Saved {0} ({1} bytes) centerRGB={2},{3},{4}" -f $outPath, $len, $sample.R, $sample.G, $sample.B)
  return ($len -gt 8000)
}

$results = @()
for ($i = 1; $i -le 3; $i++) {
  $path = Join-Path $Media ("post-baseline-0{0}.png" -f $i)
  if (Test-Path $path) { Remove-Item $path -Force }
  [WinCap2]::SetForegroundWindow($pick.Hwnd) | Out-Null
  Start-Sleep -Milliseconds 500
  # Re-pick in case HWND changed
  $pick = Pick-GameHwnd ([uint32]$proc.Id)
  $ok = Capture-Hwnd $pick.Hwnd $path
  $results += [pscustomobject]@{ file = $path; ok = [bool]$ok; title = $pick.Title }
  Start-Sleep -Seconds 2
}

try { Stop-Process -Id $proc.Id -Force -ErrorAction SilentlyContinue } catch {}
$results | ConvertTo-Json | Set-Content (Join-Path $Media "post-baseline-window-capture.json") -Encoding UTF8
Write-Host "DONE"
$results | Format-Table -AutoSize
