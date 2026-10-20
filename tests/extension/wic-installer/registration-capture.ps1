# SPDX-License-Identifier: MIT
# Compile the native registration bridge once during suite setup.
Add-Type -TypeDefinition @'
using System;
using System.ComponentModel;
using System.Runtime.InteropServices;
public static class WicRegistrationCapture {
    [DllImport("advapi32.dll")]
    public static extern int RegOverridePredefKey(IntPtr root, IntPtr target);
    [DllImport("kernel32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
    static extern IntPtr LoadLibraryW(string path);
    [DllImport("kernel32.dll", CharSet = CharSet.Ansi)]
    static extern IntPtr GetProcAddress(IntPtr module, string name);
    [DllImport("kernel32.dll")]
    static extern bool FreeLibrary(IntPtr module);
    [UnmanagedFunctionPointer(CallingConvention.StdCall)]
    delegate int RegisterServer();
    public static void Register(string path) {
        IntPtr module = LoadLibraryW(path);
        if (module == IntPtr.Zero) throw new Win32Exception();
        try {
            IntPtr address = GetProcAddress(module, "DllRegisterServer");
            if (address == IntPtr.Zero) throw new Exception("Missing registration export");
            RegisterServer register = (RegisterServer)Marshal.GetDelegateForFunctionPointer(address, typeof(RegisterServer));
            Marshal.ThrowExceptionForHR(register());
        } finally { FreeLibrary(module); }
    }
}
'@
