ARCH=$(uname -m)
LINUX_DEPLOY_ARCH=$(uname -m)

if [ "$ARCH" = "x86_64" ]; then
  ARCH="x86_64"
  LINUX_DEPLOY_ARCH="x86_64"
elif [ "$ARCH" = "aarch64" ]; then
  ARCH="arm_aarch64"
  LINUX_DEPLOY_ARCH="aarch64"
else
  echo "Unsupported architecture: $ARCH"
  exit 1
fi

curl -sSfLO "https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-$LINUX_DEPLOY_ARCH.AppImage"

chmod a+x "linuxdeploy-$LINUX_DEPLOY_ARCH.AppImage"
 
mkdir -p AppDir/usr/bin
cp BM64Recompiled AppDir/usr/bin/
cp -r assets/ AppDir/usr/bin/
cp recompcontrollerdb.txt AppDir/usr/bin/
cp icons/512.png AppDir/BM64Recompiled.png
cp .github/linux/BM64Recompiled.desktop AppDir/

"./linuxdeploy-$LINUX_DEPLOY_ARCH.AppImage" --appimage-extract
mv squashfs-root/ deploy
./deploy/AppRun --appdir=AppDir/ -d AppDir/BM64Recompiled.desktop -i AppDir/BM64Recompiled.png -e AppDir/usr/bin/BM64Recompiled

# linuxdeploy may create AppRun as a symlink to the binary; remove it first
# so we don't overwrite the actual executable through the symlink.
rm -f AppDir/AppRun
cat > AppDir/AppRun << 'APPRUN'
#!/bin/bash
this_dir="$(dirname "$(readlink -f "$0")")"
export LD_LIBRARY_PATH="$this_dir/usr/lib:$LD_LIBRARY_PATH"
if [ -f "portable.txt" ]; then
    APP_FOLDER_PATH="$PWD" exec "$this_dir/usr/bin/BM64Recompiled" "$@"
else
    cd "$this_dir/usr/bin/"
    exec ./BM64Recompiled "$@"
fi
APPRUN
chmod +x AppDir/AppRun

./deploy/usr/bin/linuxdeploy-plugin-appimage --appdir=AppDir
