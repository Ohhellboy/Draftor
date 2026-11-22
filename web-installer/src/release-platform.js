// SPDX-License-Identifier: Apache-2.0
// Capability guidance only. OS, UA and brand never authorize a Flash write.
export function desktopSerialStatus(environment) {
 const {userAgent='',platform='',mobile=false,maxTouchPoints=0,secureContext=false,serialAvailable=false,serialPolicyAllowed=true}=environment;
 if(mobile||/Android|iPhone|iPad|iPod|Mobile/i.test(userAgent)||(/Mac/i.test(platform)&&maxTouchPoints>1))return {qualified:false,reason:'MOBILE'};
 if(!secureContext)return {qualified:false,reason:'INSECURE_CONTEXT'};
 if(!serialPolicyAllowed)return {qualified:false,reason:'SERIAL_POLICY'};
 if(!serialAvailable)return {qualified:false,reason:'SERIAL_UNAVAILABLE'};
 return {qualified:true,reason:'CAPABLE_DESKTOP'};
}
// Historical name retained for callers; no Chrome brand or OS qualification.
export const qualifiedDesktopChrome=e=>desktopSerialStatus(e).qualified;
export function browserEnvironment(){return {userAgent:navigator.userAgent,platform:navigator.userAgentData?.platform||navigator.platform,mobile:navigator.userAgentData?.mobile===true,maxTouchPoints:navigator.maxTouchPoints||0,secureContext:isSecureContext,serialAvailable:typeof navigator.serial?.requestPort==='function',serialPolicyAllowed:(document.permissionsPolicy||document.featurePolicy)?.allowsFeature?.('serial')!==false};}
export const browserIsQualified=()=>desktopSerialStatus(browserEnvironment()).qualified;
export function platformMessage(language,environment=browserEnvironment()){
 const reason=desktopSerialStatus(environment).reason;
 const copy={zh:{
 MOBILE:'手机和平板不开放 USB 操作。请使用 Windows 10/11 桌面 Chrome 或 Edge；此处仍可浏览和下载。',
 INSECURE_CONTEXT:'当前页面不是安全上下文，未连接设备。请打开正式 HTTPS 网站；离线恢复请用桌面 Chrome/Edge 直接打开已保存的 HTML。不要关闭浏览器安全设置。',
 SERIAL_POLICY:'此页面的串口权限被策略限制，未连接设备。可尝试桌面 Chrome；受管理的电脑请联系管理员允许 Serial API，不要绕过组织策略。',
 SERIAL_UNAVAILABLE:'浏览器没有提供 Web Serial，未连接设备。Windows 请用最新版桌面 Chrome 或 Edge；Edge 受组织策略限制时可尝试 Chrome，或联系管理员。离线 HTML 必须直接在该浏览器打开。',
 CAPABLE_DESKTOP:'当前桌面环境具备 Web Serial。Windows Chrome/Edge 测试性开放，真实 USB 路径仍待现场验证（WINDOWS_DEVICE_FIELD_TEST_PENDING）；macOS Chrome 继承既有验收。安装前必须保存并回选完整 ZIP，另存离线 HTML。'
 },en:{
 MOBILE:'USB operations are unavailable on phones/tablets. Use Windows 10/11 desktop Chrome or Edge. Reading and downloads remain available.',
 INSECURE_CONTEXT:'This page is not a secure context; no device connected. Use the official HTTPS site, or open the saved offline HTML directly in desktop Chrome/Edge. Do not disable browser security.',
 SERIAL_POLICY:'Serial access is restricted by policy; no device connected. Try desktop Chrome or ask your administrator to allow the Serial API. Do not bypass organizational policy.',
 SERIAL_UNAVAILABLE:'Web Serial is unavailable; no device connected. On Windows use current desktop Chrome or Edge. If Edge is managed, try Chrome or ask your administrator. Open the offline HTML directly in that browser.',
 CAPABLE_DESKTOP:'Web Serial is available. Windows Chrome/Edge is open for informed beta use; real USB field testing remains pending (WINDOWS_DEVICE_FIELD_TEST_PENDING). macOS Chrome inherits prior qualification. Save and reselect the complete ZIP and keep the offline HTML before installation.'
 }};return copy[language==='en'?'en':'zh'][reason];
}
export function publicErrorCode(error){if(/^[A-Z_0-9]{1,48}$/.test(error?.code||''))return error.code;if(error?.name==='NotFoundError')return 'PORT_SELECTION_CANCELLED';if(['SecurityError','NotAllowedError'].includes(error?.name))return 'SERIAL_PERMISSION_DENIED';return 'IO';}
export function serialErrorHelp(code,language){const en=language==='en';
 if(code==='SERIAL_PERMISSION_DENIED')return en?'Serial permission was denied or blocked by browser/organization policy. No automatic retry. Use site permissions, try desktop Chrome, or ask your administrator.':'串口授权被拒绝或被浏览器／组织策略限制，不会自动重试。请检查网站权限，尝试桌面 Chrome，或联系管理员。';
 if(code==='PORT_SELECTION_CANCELLED')return en?'Port selection cancelled; no device connected. Close this session before starting again.':'已取消串口选择，尚未连接设备。关闭本次会话后可主动重新开始。';
 if(code==='CONNECTION')return en?'The port could not be opened. Close serial monitors/other installer tabs; check the data cable, BOOT download mode and Device Manager. Do not guess a COM port or keep retrying a communication fault.':'串口未能打开。请关闭串口监视器或其他安装页，检查数据线、BOOT 下载模式及设备管理器。不要猜测 COM 端口，也不要对通信异常连续重试。';
 return '';
}
