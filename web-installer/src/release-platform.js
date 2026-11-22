// SPDX-License-Identifier: Apache-2.0
// Support policy only. Device identity/security are independently checked by ROM.
export function qualifiedDesktopChrome(environment) {
  const {userAgent='',vendor='',platform='',brands,secureContext=false,serialAvailable=false,brave=false}=environment;
  const mac=/Macintosh/.test(userAgent)&&(/Mac/.test(platform)||platform==='macOS');
  const chrome=/Chrome\/\d+/.test(userAgent)&&vendor==='Google Inc.'&&!brave&&!/(Edg|OPR|Opera|Android|Mobile|CriOS|FxiOS)/.test(userAgent);
  const brandOk=!brands||brands.some(x=>x.brand==='Google Chrome');
  return Boolean(mac&&chrome&&brandOk&&secureContext&&serialAvailable);
}
export function browserIsQualified() {
  return qualifiedDesktopChrome({userAgent:navigator.userAgent,vendor:navigator.vendor,platform:navigator.userAgentData?.platform||navigator.platform,
    brands:navigator.userAgentData?.brands,secureContext:isSecureContext,serialAvailable:!!navigator.serial?.requestPort,brave:!!navigator.brave});
}
