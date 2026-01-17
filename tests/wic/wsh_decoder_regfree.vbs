' Windows Script Host test for registration-free Libsixel.Decoder automation.
' This script expects cscript.exe.manifest and libwicsixel.dll beside it.
'
' Usage:
'   cscript //nologo wsh_decoder_regfree.vbs C:\path\to\snake.six
'
Option Explicit

Const DPI_DEFAULT = 96

Dim decoder
Set decoder = CreateObject("Libsixel.Decoder")
If decoder Is Nothing Then
    WScript.Echo "ERROR: CreateObject failed"
    WScript.Quit 1
End If

Dim inputPath
If WScript.Arguments.Count > 0 Then
    inputPath = WScript.Arguments(0)
Else
    WScript.Echo "ERROR: missing input path"
    WScript.Quit 1
End If

Dim sixelText
sixelText = ReadAllText(inputPath)

Dim picture
Set picture = decoder.LoadFromString(sixelText)
If picture Is Nothing Then
    WScript.Echo "ERROR: decode returned nothing"
    WScript.Quit 1
End If

Dim widthPx
Dim heightPx
widthPx = HimetricToPixels(picture.Width, DPI_DEFAULT)
heightPx = HimetricToPixels(picture.Height, DPI_DEFAULT)

If widthPx <= 0 Or heightPx <= 0 Then
    WScript.Echo "ERROR: invalid dimensions " & widthPx & "x" & heightPx
    WScript.Quit 1
End If

WScript.Echo "OK " & widthPx & " " & heightPx
WScript.Quit 0

Function ReadAllText(path)
    Dim stream

    Set stream = CreateObject("ADODB.Stream")
    stream.Type = 2 'adTypeText
    stream.Charset = "utf-8"
    stream.Open
    stream.LoadFromFile path
    ReadAllText = stream.ReadText
    stream.Close
    Set stream = Nothing
End Function

Function HimetricToPixels(himetric, dpi)
    HimetricToPixels = CLng((himetric * dpi + 1270) / 2540)
End Function
