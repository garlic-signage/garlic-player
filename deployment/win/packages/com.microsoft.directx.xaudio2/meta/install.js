function Component(){}

Component.prototype.createOperations = function()
{
    try
    {
        if (systemInfo.kernelType === "winnt"  && majorVersion(systemInfo.kernelVersion) <= 6)
        {
            component.createOperations();
            component.addElevatedOperation("Execute", "@TargetDir@/DXSETUP.exe", "/silent", "/norestart");
            component.addElevatedOperation("Delete", "@TargetDir@/DSETUP.dll");
            component.addElevatedOperation("Delete", "@TargetDir@/dsetup32.dll");
            component.addElevatedOperation("Delete", "@TargetDir@/dxdllreg_x86.cab");
            component.addElevatedOperation("Delete", "@TargetDir@/DXSETUP.exe");
            component.addElevatedOperation("Delete", "@TargetDir@/dxupdate.cab");
            component.addElevatedOperation("Delete", "@TargetDir@/Jun2010_XAudio_x64.cab");
            component.addElevatedOperation("Delete", "@TargetDir@/Jun2010_XAudio_x86.cab");
        }
    }
    catch (e)
    {
        print(e);
    }
}
