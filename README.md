This is a sip client using the 2 FXS ports available on routers based on the Infineon Danube (lantiq xway) and running openwrt.

Based on the [midge](http://zftlab.org/pages/2014070600.html) package [oem-voip](https://github.com/ZigFisher/Midge/tree/master/package/oem-voip) and [sofia-sip](http://sofia-sip.sourceforge.net).

See the [INSTRUCTIONS](INSTRUCTIONS.md) for instructions.

Precompiled binaries for _attitude adjustment_, _barrier breaker_ and _chaos calmer_ are available [here](https://drive.google.com/folderview?id=0BwPmW2whNqGlcHVuUHd1Z2xWUjA&usp=sharing), binaries for openwrt 19.07.2
and 23.05.4 are available as github [releases](https://github.com/olivluca/danube-voip/releases).

**Beware** openwrt 23.05.4 is too big for this device and it won't be
possible to install these packages and their dependencies in a stock image,
so you either have to [extend the writable partition](https://openwrt.org/docs/guide-user/additional-software/extroot_configuration)
(I never tried) or build a custom image with many things removed (I provide
one for testing in the release but it may not fit your needs, see the
workflow for details).

Also, with openwrt 23.05.4 make sure to use the sofia-sip package provided
here, the one from the telephony feed has been compiled with stun disabled.

**With openwrt 24.10** I couldn't find a way to build an image including sofia-sip/svd/luci-app-svd, the only option is extroot, and even then it's impossible 
to install the packages required for extroot on the stock image, hence in the workflow I create a stripped down image with the packages for extroot included.


