function cancelInstaller(message)
{
    installer.setDefaultPageVisible(QInstaller.Introduction, false);
    installer.setDefaultPageVisible(QInstaller.TargetDirectory, false);
    installer.setDefaultPageVisible(QInstaller.ComponentSelection, false);
    installer.setDefaultPageVisible(QInstaller.ReadyForInstallation, false);
    installer.setDefaultPageVisible(QInstaller.StartMenuSelection, false);
    installer.setDefaultPageVisible(QInstaller.PerformInstallation, false);
    installer.setDefaultPageVisible(QInstaller.LicenseCheck, false);

    var abortText = "<font color='red'>" + message +"</font>";
    installer.setValue("FinishedText", abortText);
}

function Component()
{
    if ( systemInfo.currentCpuArchitecture === "i386")
    {
        cancelInstaller("Installation on " + systemInfo.currentCpuArchitecture + " architecture is not supported");
    }
}

Component.prototype.createOperations = function()
{
    component.createOperations();

    if (systemInfo.productType === "windows")
	{
        component.addOperation("CreateShortcut", "@TargetDir@/start_garlic.bat", "@StartMenuDir@/garlic-player.lnk",
            "workingDirectory=@TargetDir@",
			"iconPath=@TargetDir@/bin/garlic-player.ico"
        );
        component.addOperation("CreateShortcut", "@TargetDir@/start_garlic.bat", "@DesktopDir@/garlic-player.lnk",
            "workingDirectory=@TargetDir@",
			"iconPath=@TargetDir@/bin/garlic-player.ico"
        );
    }
}