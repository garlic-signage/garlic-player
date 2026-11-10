TEMPLATE = aux

INSTALLER = garlic_install

INPUT = $$PWD/config/config.xml $$PWD/packages
garlicinstall.input = INPUT
garlicinstall.output = $$INSTALLER
unix:garlicinstall.commands = /home/niko/Anwendungen/Qt/Tools/QtInstallerFramework/2.0/bin/binarycreator -c $$PWD/config/config.xml -p $$PWD/packages ${QMAKE_FILE_OUT}
win32:garlicinstall.commands = C:\Qt\Tools\QtInstallerFramework\2.0\bin\binarycreator -c $$PWD\config\config.xml -p $$PWD\packages ${QMAKE_FILE_OUT}
garlicinstall.CONFIG += target_predeps no_link combine

QMAKE_EXTRA_COMPILERS += garlicinstall

DISTFILES += \
    packages/com.sagiadinos.garlic.x64/data/content_to_install.txt \
    packages/com.sagiadinos.garlic.x64/meta/agpl-3.0.txt \
    packages/com.sagiadinos.garlic.x64/meta/install.js
    config/controllerScript.js \
    packages/com.microsoft.directx.xaudio2/meta/install.js \
    packages/com.microsoft.vcredist.x64/meta/install.js \
    packages/com.microsoft.directx.xaudio2/data/dx_sdk2010_xaudio2_redist/dxdllreg_x86.cab \
    packages/com.microsoft.directx.xaudio2/data/dx_sdk2010_xaudio2_redist/dxupdate.cab \
    packages/com.microsoft.directx.xaudio2/data/dx_sdk2010_xaudio2_redist/Jun2010_XAudio_x64.cab \
    packages/com.microsoft.directx.xaudio2/data/dx_sdk2010_xaudio2_redist/Jun2010_XAudio_x86.cab \
    packages/com.microsoft.directx.xaudio2/data/dx_sdk2010_xaudio2_redist/DSETUP.dll \
    packages/com.microsoft.directx.xaudio2/data/dx_sdk2010_xaudio2_redist/dsetup32.dll \
    packages/com.microsoft.directx.xaudio2/data/dx_sdk2010_xaudio2_redist/DXSETUP.exe \
    packages/com.microsoft.vcredist.x64/data/vc_redist.x64.exe \
    packages/com.microsoft.directx.xaudio2/meta/package.xml \
    packages/com.microsoft.vcredist.x64/meta/package.xml \
    packages/com.sagiadinos.garlic.x64/meta/package.xml \
    packages/com.microsoft.directx.xaudio2/data/DXSDK2010_XAudio2_redist/README.txt

