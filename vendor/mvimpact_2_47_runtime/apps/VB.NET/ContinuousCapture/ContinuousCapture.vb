Imports System.Threading

Imports mv.impact.acquire
Imports mv.impact.acquire.examples.helper
#If USE_DISPLAY Then
Imports mv.impact.acquire.display
#End If '#If USE_DISPLAY Then

Class MyThread
    Public pDev As Device
#If USE_DISPLAY Then
    Public window As ImageDisplayWindow
#End If '#If USE_DISPLAY Then
    Public terminated As Boolean = False

    Public Sub New(ByRef d As Device)
        pDev = d
    End Sub

    Public Sub CaptureThread()
        ' establish access to the statistic properties
        Dim statistics = New Statistics(pDev)
        ' create an interface to the device found
        Dim fi = New FunctionInterface(pDev)

        ' Send all requests to the capture queue. There can be more than 1 queue for some devices, but for this sample
        ' we will work with the default capture queue. If a device supports more than one capture or result
        ' queue, this will be stated in the manual. If nothing is mentioned about it, the device supports one
        ' queue only. This loop will send all requests currently available to the driver. To modify the number of requests
        ' use the property mv.impact.acquire.SystemSettings.requestCount at runtime or the property
        ' mv.impact.acquire.Device.defaultRequestCount BEFORE opening the device.
        Dim result As TDMR_ERROR = TDMR_ERROR.DMR_NO_ERROR
        Dim requestCount As Integer = fi.requestCount
        For i As Integer = 0 To requestCount - 1
            result = fi.imageRequestSingle()
            If (result <> TDMR_ERROR.DMR_NO_ERROR) Then
                Console.WriteLine("'FunctionInterface.imageRequestSingle' returned with an unexpected result: {0}({1})", result, ImpactAcquireException.getErrorCodeAsString(result))
            End If
        Next i

        DeviceAccess.manuallyStartAcquisitionIfNeeded(pDev, fi)
        ' run thread loop
        Dim pRequest As Request = Nothing
        ' we always have to keep at least 2 images as the display module might want to repaint the image, thus we
        ' cannot free it unless we have a assigned the display to a new buffer.
        Dim pPreviousRequest As Request = Nothing
        Dim timeout_ms As Integer = 500
        Dim cnt As Integer = 0
        Dim requestNr As Integer = mv.impact.acquire.Device.INVALID_ID
        While (Not terminated)
            ' wait for results from the default capture queue
            requestNr = fi.imageRequestWaitFor(timeout_ms)
            If (fi.isRequestNrValid(requestNr)) Then
                pRequest = fi.getRequest(requestNr)
            Else
                pRequest = Nothing
            End If

            If (Not pRequest Is Nothing) Then
                If (pRequest.isOK) Then
                    cnt = cnt + 1
                    ' here we can display some statistical information every 100th image
                    If (cnt Mod 100 = 0) Then
                        Console.WriteLine("Info from {0}: {1}: {2}, {3}: {4}, {5}: {6}", pDev.serial.read(), statistics.framesPerSecond.name, statistics.framesPerSecond.readS(), statistics.errorCount.name, statistics.errorCount.readS(), statistics.captureTime_s.name, statistics.captureTime_s.readS())
                    End If
#If USE_DISPLAY Then
#If CLR_AT_LEAST_3_DOT_5 Then
                    ' Extension methods are not supported by CLR versions smaller than 3.5 and this next function
                    ' is therefore not available then.
                    window.imageDisplay.SetImage(pRequest)
#Else
                    ' If extension methods are not available, the following function can be used instead. It is
                    ' not as convenient and will only work for some pixel formats. For more complex pixel formats
                    ' use other overloads of this method
                    window.imageDisplay.SetImage(pRequest.imageData.read(), pRequest.imageWidth.read(), pRequest.imageHeight.read(), pRequest.imageBytesPerPixel.read() * 8, pRequest.imageLinePitch.read())
#End If
                    window.imageDisplay.Update()
#End If '#If USE_DISPLAY Then
                Else
                    Console.WriteLine("Error: {0}", pRequest.requestResult.readS())
                End If
                If (Not pPreviousRequest Is Nothing) Then
                    ' this image has been displayed thus the buffer is no longer needed...
                    pPreviousRequest.unlock()
                End If
                pPreviousRequest = pRequest
                ' send a new image request into the capture queue
                fi.imageRequestSingle()
                'Else
                ' Please note that slow systems or interface technologies in combination with high resolution sensors
                ' might need more time to transmit an image than the timeout value which has been passed to imageRequestWaitFor().
                ' If this is the case simply wait multiple times OR increase the timeout(not recommended as usually not necessary
                ' and potentially makes the capture thread less responsive) and rebuild this application.
                ' Once the device is configured for triggered image acquisition and the timeout elapsed before
                ' the device has been triggered this might happen as well.
                ' The return code would be -2119(DEV_WAIT_FOR_REQUEST_FAILED) in that case, the documentation will provide
                ' additional information under TDMR_ERROR in the interface reference.
                ' If waiting with an infinite timeout(-1) it will be necessary to call 'imageRequestReset' from another thread
                ' to force 'imageRequestWaitFor' to return when no data is coming from the device/can be captured.
                ' Console.WriteLine("imageRequestWaitFor failed ({0}, {1}), timeout value too small?", requestNr, ImpactAcquireException.getErrorCodeAsString(requestNr))
                End If
        End While
        DeviceAccess.manuallyStopAcquisitionIfNeeded(pDev, fi)

#If USE_DISPLAY Then
        ' stop the display from showing freed memory
        window.imageDisplay.SetImage(IntPtr.Zero, 0, 0, 0, 0)
#End If '#If USE_DISPLAY Then
        ' In this sample all the next lines are redundant as the device driver will be
        ' closed now, but in a real world application a thread like this might be started
        ' several times an then it becomes crucial to clean up correctly.

        ' free the last potentially locked request
        If (Not pRequest Is Nothing) Then
            pRequest.unlock()
        End If

        ' clear the request queue
        fi.imageRequestReset(0, 0)
    End Sub
End Class

Module Module1
    Sub Main()
        mv.impact.acquire.LibraryPath.init() ' this will add the folders containing unmanaged libraries to the PATH variable.
        Dim pDev As Device = DeviceAccess.getDeviceFromUserInput()
        If (pDev Is Nothing) Then
            Console.WriteLine("Unable to continue! Press any key to end the program.")
            Console.Read()
            Environment.Exit(1)
        End If

        Console.WriteLine("Initialising the device. This might take some time...")
        Try
            pDev.open()
        Catch e As ImpactAcquireException
            ' this e.g. might happen if the same device is already opened in another process...
            Console.WriteLine("An error occurred while opening device " + pDev.serial.read() + "(error code: " + e.Message + "). Press any key to end the application...")
            Console.ReadLine()
            Environment.Exit(1)
        End Try

        Dim threadData As New MyThread(pDev)
#If USE_DISPLAY Then
        threadData.window = New ImageDisplayWindow(String.Format("mvIMPACT_acquire sample, Device {0}", pDev.serial.read()))
#End If '#If USE_DISPLAY Then
        Dim thread As Thread = New Thread(New ThreadStart(AddressOf threadData.CaptureThread))
        thread.Start()
        Console.ReadLine()
        threadData.terminated = True
        thread.Join()
#If USE_DISPLAY Then
        threadData.window.Dispose()
#End If '#If USE_DISPLAY Then
    End Sub
End Module
