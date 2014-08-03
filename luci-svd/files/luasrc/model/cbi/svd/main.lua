m=Map("svd",translate("Main"), translate("Here you can configure the general options."))

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
s=m:section(TypedSection,"main", translate("Main"))
s.addremove = false
s.anonymous = true

loglevel=s:option(ListValue,"log_level", translate("Log level"), translate("From 0 (quiet) to 9 (very verbose)."))
loglevel.optional=true
loglevel.rmempty=true
for i=0,9 do
   loglevel:value(tostring(i),tostring(i))
end
loglevel:value("",translate("--remove--"))

rtp_first=s:option(Value,"rtp_port_first", translate("First rtp port"), translate("Voip will try to bind rtp ports starting from this one."))
rtp_first.optional=false
rtp_first.rmempty=false
rtp_first.datatype="port"

rtp_last=s:option(Value,"rtp_port_last", translate("Last rtp port"), translate("This is the last rtp port voip will try to use."))
rtp_last.optional=false
rtp_last.rmempty=false
rtp_last.datatype="port"
function rtp_last.validate(self,value,sid)
  f=self.section.fields.rtp_port_first
  rtp_first=self.map:formvalue(f:cbid(sid))
  rtp_last=value
  if rtp_first and rtp_last then
    if tonumber(rtp_last)<tonumber(rtp_first)  then
      return nil,translate("last rtp port must be >= first rtp port")
    end
  end  
  return value  
end

sip_tos=s:option(Value,"sip_tos", translate("Tos for sip traffic"))
sip_tos.optional=false
sip_tos.rmempty=false
sip_tos.datatype="range(0,255)"

rtp_tos=s:option(Value,"rtp_tos", translate("Tos for rtp traffic"))
rtp_tos.optional=false
rtp_tos.rmempty=false
rtp_tos.datatype="range(0,255)"

led=s:option(ListValue,"led", translate("Voip led"), translate("Name of the led to activate when at least one account has been registered."))
for k, v in ipairs(leds) do
  led:value(v)
end
led:value("",translate("--remove--"))  
led.optional=true
led.rmempty=true

return m
