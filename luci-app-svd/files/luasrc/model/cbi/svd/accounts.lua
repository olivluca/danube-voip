local util = require("luci.util")
local class      = util.class
DoubleBox=class(AbstractValue)

function DoubleBox.__init__(self, ...)
    AbstractValue.__init__(self, ...)
    self.template  = "svd/doublebox"
    self.keylist = {}
    self.vallist = {}
end

function DoubleBox.reset_values(self)
    self.keylist = {}
    self.vallist = {}
end
                   
function DoubleBox.value(self, key, val)
    val = val or key
    table.insert(self.keylist, tostring(key))
    table.insert(self.vallist, tostring(val))
end

function DoubleBox.valueofkey(self, key)
    for i,v in ipairs(self.keylist) do
      if v == key then
        return self.vallist[i]
      end
    end
    return "?"        
end

function DoubleBox.valuelist(self, section)
    local val = self:cfgvalue(section)
    if not(type(val) == "string") then
       return {}
    end
    return luci.util.split(val, " ")
end 
                                                
ArrayValue=class(Value)
function ArrayValue.__init__(self, ...)
    Value.__init__(self, ...)
    self.template  = "svd/arrayvalue"
    self.captionlist = {}
end

function ArrayValue.caption(self, cap)
    table.insert(self.captionlist, tostring(cap))
end

function ArrayValue.valuelist(self, section)
    local default = {}
    for k in pairs(self.captionlist) do
      table.insert(default,"")
    end  
    local val = self:cfgvalue(section) or self.default
    if not(type(val) == "string") then
       return default
    end
    if (val=="") then
       return default
    end  
    return luci.util.split(val, " ")
end 

function ArrayValue.getcaption(self,index)
   return self.captionlist[index]
end   

FlagArray=class(AbstractValue)
function FlagArray.__init__(self, ...)
    AbstractValue.__init__(self, ...)
    self.template  = "svd/farray"
    self.enabled = "on"
    self.disabled = "off"
    self.captionlist = {}
end

function FlagArray.caption(self, cap)
    table.insert(self.captionlist, tostring(cap))
end

function FlagArray.valuelist(self, section)
    local default = {}
    for k in pairs(self.captionlist) do
      table.insert(default,self.enabled)
    end  
    local val = self:cfgvalue(section) or self.default
    if not(type(val) == "string") then
       return default
    end
    if (val=="") then
       return default
    end  
    return luci.util.split(val, " ")
end 

function FlagArray.getcaption(self,index)
   return self.captionlist[index]
end   


m=Map("svd",translate("Accounts"), translate("Here you can configure the sip accounts. At least one must be defined."))
s=m:section(TypedSection,"account", translate("Accounts"))
s.addremove = true

disabled=s:option(Flag, "disabled", translate("Disable account"), translate("A disabled account won't be registered and won't be used for outgoing calls."))
disabled.enabled="on"
disabled.disabled="off"
disabled.optional=true
disabled.rmempty=true

username=s:option(Value,"user", translate("User name"), translate("User to build the sip address."))
username.optional=false
username.rmempty=false
username.datatype="minlength(1)"

auth_name=s:option(Value,"auth_name",translate("Auth name"), translate("Name to use to authenticate with the server. If not defined it will be the same as 'User name'."))
auth_name.optional=true
auth_name.rmempty=true

display_name=s:option(Value,"display", translate("Display Name"), translate("Optional display name."))
display_name.optional=true
display_name.rmempty=true

password=s:option(Value,"password",translate("Password"))
password.password=true
          
domain=s:option(Value,"domain", translate("Sip domain"), translate("Domain used to build the sip address."))
domain.datatype="host"
domain.optional=false
domain.rmempty=false
    
registrar=s:option(Value,"registrar",translate("Registrar"), translate("Host name where to register the account. If not defined it will be the same as 'Sip domain'."))
registrar.optional=true
registrar.rmempty=true

proxy=s:option(Value,"outbound_proxy",translate("Outbound proxy"), translate("Host name to use as sip outbound proxy. If not defined, no proxy will be used."))
proxy.optional=true
proxy.rmempty=true;

dtmf=s:option(ListValue,"dtmf", translate("Dtmf type"), translate("How to send dtmf during a call. If not defined it will be rfc2883"))
dtmf.optional=true
dtmf.rmempty=true
dtmf:value("rfc2883","rfc2883")
dtmf:value("info","info")
dtmf:value("off",translate("disabled"))
dtmf:value("",translate("--remove--"))

ring=s:option(FlagArray,"ring", translate("Ring on incoming call"), translate("Select the channels that will ring when a call comes from this account."))
ring.optional=false;
ring.rmempty=false
for i=1,2 do
  ring:caption(translatef("channel %d",i))
end  

pri=s:option(ArrayValue,"priority",translate("Outgoing priority"), translate("When an outgoing call is made, if no dialplan entry matches the lowest priority account will be used. Zero means the account won't be used."))
pri.optional=true;
pri.rmempty=true;
pri.size=5
pri.datatype='or(max(1000), "")'
for i=1,2 do
  pri:caption(translatef("channel %d",i))
end
function pri:validate(value)
 return value
end
function pri:transform(value)
 return value
end  

codecs=s:option(DoubleBox,"codecs",translate("Codecs"), translate("Select the codecs to be used with this account and their priority. If not defined, all codecs will be available."))
codecs.optional=true
codecs.rmmempty=true
codecs:value("G722","G722")
codecs:value("PCMA","PCMA")
codecs:value("G729","G729")
codecs:value("G729E","G729E")
codecs:value("G723","G723")
codecs:value("iLBC","iLBC")
codecs:value("G726-16","G726-16")
codecs:value("G726-32","G726-32")
codecs:value("G726-40","G726-40")

agent=s:option(Value,"user_agent",translate("User agent"), translate("Optional user agent to be used instead of the default. Some sip server only accept registration made with a predefined user agent."))
agent.optional=true
agent.rmempty=true

return m
