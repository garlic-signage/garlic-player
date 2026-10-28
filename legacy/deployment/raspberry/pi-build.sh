
# Delete from Stage 2 01-sys-tweaks/00-packages
# build-essential manpages-dev
# delete 03-accept-mathematica-eula
# delete ExportImage

# Delete from Stage 3 00-packages
# gstreamer1.0-x gstreamer1.0-omx gstreamer1.0-plugins-base gstreamer1.0-plugins-good gstreamer1.0-plugins-bad gstreamer1.0-alsa gstreamer1.0-libav
# chromium-browser rpi-chromium-mods libwidevinecdm0
# firefox rpi-firefox-mods
# gtk2-engines desktop-base gnome-themes-extra-data gnome-icon-theme

export IMG_NAME="garlic-player"
export PI_GEN_RELEASE="Garlic-Player - SMIL based Digital Signage Player "
export BASE_DIR=/home/niko/pi-gen
export WORK_DIR=/home/niko/build_pi_image
export TARGET_HOSTNAME="garlic-player-rpi"
export ENABLE_CACHING=1
export USE_CACHED_DATA=1
#export ENABLE_SSH=1 must be 0 otherwise piwiz will behave ill
export STAGE_LIST="stage0 stage1 stage2 stage3 stage4N"

touch /home/niko/pi-gen/stage4/SKIP /home/niko/pi-gen/stage5/SKIP

mkdir -p $WORK_DIR

cd /home/niko/pi-gen

bash ./build.sh
