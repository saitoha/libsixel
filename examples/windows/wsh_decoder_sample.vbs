' Windows Script Host sample for Libsixel.Decoder automation
'
' Usage:
'   cscript //nologo examples\windows\wsh_decoder_sample.vbs \
'       C:\\path\\to\\image.png C:\\path\\to\\image.six
'
Option Explicit

Const DPI_DEFAULT = 96

Dim decoder
Set decoder = CreateObject("Libsixel.Decoder")
If decoder Is Nothing Then
    WScript.Echo "Failed to create Libsixel.Decoder"
    WScript.Quit 1
End If

Dim inputImagePath
Dim sixelTextPath

If WScript.Arguments.Count > 0 Then
    inputImagePath = WScript.Arguments(0)
Else
    inputImagePath = "images\\snake.png"
End If

If WScript.Arguments.Count > 1 Then
    sixelTextPath = WScript.Arguments(1)
Else
    sixelTextPath = "images\\snake.six"
End If

WScript.Echo "Input image (binary formats): " & inputImagePath
WScript.Echo "Input SIXEL text: " & sixelTextPath
WScript.Echo ""

DumpPicture "LoadFromFile", decoder.LoadFromFile(inputImagePath)
DumpPicture "LoadFromStream (ADODB.Stream)", _
    decoder.LoadFromStream(OpenStream(inputImagePath))
DumpPicture "LoadFromByteArray", _
    decoder.LoadFromByteArray(ReadAllBytes(inputImagePath))
DumpPicture "LoadFromString (SIXEL text)", _
    decoder.LoadFromString(ReadAllText(sixelTextPath))

WScript.Quit 0

Function OpenStream(path)
    Dim stream

    Set stream = CreateObject("ADODB.Stream")
    stream.Type = 1 'adTypeBinary
    stream.Open
    stream.LoadFromFile path
    Set OpenStream = stream
End Function

Function ReadAllBytes(path)
    Dim stream

    Set stream = OpenStream(path)
    ReadAllBytes = stream.Read
    stream.Close
    Set stream = Nothing
End Function

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

Function DumpPicture(label, picture)
    Dim widthPx
    Dim heightPx

    If picture Is Nothing Then
        WScript.Echo label & ": failed to decode"
        Exit Function
    End If

    widthPx = HimetricToPixels(picture.Width, DPI_DEFAULT)
    heightPx = HimetricToPixels(picture.Height, DPI_DEFAULT)

    WScript.Echo label & ": " & widthPx & " x " & heightPx & _
        " px (using " & DPI_DEFAULT & " DPI conversion)"
End Function

Function HimetricToPixels(himetric, dpi)
    HimetricToPixels = CLng((himetric * dpi + 1270) / 2540)
End Function
