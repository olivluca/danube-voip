local uci = require "luci.model.uci"
local cursor = uci.cursor()

m=Map("svd",translate("Dialplan"), translate("Here you can configure how to route outgoing calls"))
function m.on_commit(self)
  luci.sys.call("/etc/init.d/svd stop")
  luci.sys.call("/etc/init.d/svd start")
end
s=m:section(TypedSection,"dialplan")
s.addremove = true
s.template="cbi/tblsection"
s.sortable=true
s.anonymous=true

prefix=s:option(Value,"prefix",translate("Prefix"),translate("of the dialled number"))
prefix.datatype="and(phonedigit,minlength(1))"
prefix.optional=false
prefix.rmempty=false
replace=s:option(Value,"replace",translate("Replace with"))
remove_prefix=s:option(Flag,"remove_prefix",translate("Remove prefix"),translate("(only if replace is unset)"))
account=s:option(ListValue,"account",translate("Account"),translate("Select the account to use"))
account:value("","")
account.datatype="minlength(1)"
account.optional=false
account.rmempty=false
cursor:foreach("svd","account", function (s)
  account:value(s[".name"],s[".name"])
end)
return m
