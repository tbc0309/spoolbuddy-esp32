# Upstream UI resources

`lucide/wifi-off.svg` is copied without geometry changes from Lucide 0.555.0,
the version pinned by Bambuddy's frontend package-lock.json. Its license is
retained alongside it. The generator only substitutes currentColor and renders
the upstream TopBar's 20px size for LVGL.

Source: https://github.com/lucide-icons/lucide/blob/0.555.0/icons/wifi-off.svg

The connected signal indicator is the four CSS bars in Bambuddy
`frontend/src/components/spoolbuddy/SpoolBuddyTopBar.tsx`: 4px widths, 2px gaps,
4/8/12/16px heights, rounded corners and white fill. No RSSI scale has been
invented: upstream renders all four bars when online. The port retains its
existing distinction between Wi-Fi connectivity and backend connectivity.

`tools/generate_ui_assets.cjs` creates LVGL raster images from this geometry;
the original 40px Wi-Fi illustration within pages remains unchanged.
