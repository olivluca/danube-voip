m=Map("svd",translate("Channels"),translate("Here you can configure each channel options."))

local sysfs_path = "/sys/class/leds/"
local leds = {}

local fs   = require "nixio.fs"
local util = require "nixio.util"

if fs.access(sysfs_path) then
  leds = util.consume((fs.dir(sysfs_path)))
end


function m.on_commit(self)
  luci.sys.call("/etc/init.d/svd stop")
  luci.sys.call("/etc/init.d/svd start")
end

for chan=1,2 do
  s=m:section(NamedSection,tostring(chan), "channel", translatef("channel %d",chan))
  s.addremove = true
  s.optional = true

  enc_db=s:option(Value,"enc_db", translate("Enc db"), translate("Enc db (default 0)"))
  enc_db.optional=true
  enc_db.rmempty=true
  enc_db.datatype="uinteger"

  dec_db=s:option(Value,"dec_db", translate("Dec db"), translate("Dec db (default 0)"))
  dec_db.optional=true
  dec_db.rmempty=true
  dec_db.datatype="uinteger"
  
  vad=s:option(ListValue,"vad", translate("Vad"), translate("Voice activity detector (default off)"))
  vad.optional=true
  vad.rmmempty=true
  vad:value("off",translate("disabled"))
  vad:value("on",translate("on/comfort noise and spectral information"))
  vad:value("g711",translate("g711/comfort noise without spectral information"))
  vad:value("cng_only",translate("comfort noise generation only"))
  vad:value("sc_only",translate("Sc only"))
  vad:value("",translate("--remove--"))
  
  hpf=s:option(Flag,"hpf",translate("Hpf"),translate("High pass filter"))
  hpf.disabled="off"
  hpf.enabled="on"
  
  wlec_type=s:option(ListValue,"wlec_type",translate("Wlec type"),translate("Echo cancellation type (default off)"))
  wlec_type.optional=true
  wlec_type.rmempty=true
  wlec_type:value("off",translate("disabled"))
  wlec_type:value("ne",translate("Near end"))
  wlec_type:value("nfe",translate("Near and far end"))
  wlec_type:value("",translate("--remove--"))
  
  wlec_nlp=s:option(Flag,"wlec_nlp",translate("Wlec nlp"),translate("Wlec non linear processing"))
  wlec_nlp.disabled="off"
  wlec_nlp.enabled="on"
  wlec_nlp:depends("wlec_type","ne")
  wlec_nlp:depends("wlec_type","nfe")
  
  wlec_ne_nb=s:option(Value,"wlec_ne_nb",translate("Wlec near end window"),translate("default 4"))
  wlec_ne_nb.optional=true
  wlec_ne_nb.rmempty=true
  wlec_ne_nb.datatype="uinteger"  
  wlec_ne_nb:depends("wlec_type","ne")
  wlec_ne_nb:depends("wlec_type","nfe")

  wlec_fe_nb=s:option(Value,"wlec_fe_nb",translate("Wlec far end window"),translate("default 4"))
  wlec_fe_nb.optional=true
  wlec_fe_nb.rmempty=true
  wlec_fe_nb.datatype="uinteger"  
  wlec_fe_nb:depends("wlec_type","nfe")
  
  cid=s:option(ListValue,"cid",translate("Caller id"),translate("The caller id protocol (default etsi fsk)"))
  cid.optional=true
  cid.rmempty=true
  cid:value("off",translate("disabled"))
  cid:value("telcordia","telcordia")
  cid:value("etsi_fsk","etsi fsk")
  cid:value("etsi_dtmf","etsi dtmf")
  cid:value("sin","sin")
  cid:value("ntt","ntt")
  cid:value("kpn_dtmf","kpn dtmf")
  cid:value("kpn_dtmf_fsk","kpn dtmf fsk")
  cid:value("",translate("--remove--"))
  
  led=s:option(ListValue,"led", translate("Led"), translate("Name of the led associated to this channel."))
  for k, v in ipairs(leds) do
    led:value(v)
  end
  led:value("",translate("--remove--"))  
  led.optional=true
  led.rmempty=true
end  

return m
