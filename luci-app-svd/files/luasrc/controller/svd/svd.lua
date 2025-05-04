module("luci.controller.svd.svd", package.seeall)

function index()
   page = node("admin", "svd")
   page.target = firstchild()
   page.title  = translate("Voip")
   page.order  = 90
   page.acl_depends = { "luci-app-svd" }
                                  
   entry({"admin","svd","state"}, template("svd/status"),translate("Status"),20) 
   entry({"admin","svd","main"}, cbi("svd/main"),translate("Main"), 30)
   entry({"admin","svd","channels"}, cbi("svd/channels"),translate("Channels"), 40)
   entry({"admin","svd","accounts"}, cbi("svd/accounts"),translate("Accounts"), 50)
   entry({"admin","svd","dialplan"}, cbi("svd/dialplan"),translate("Dialplan"), 60)
   entry({"admin", "svd", "status"}, call("svd_get_status")).leaf = true
end
     
function svd_get_status()
   luci.http.prepare_content("application/json")
   luci.http.write('{ "regs" : ' ..  luci.sys.exec("echo get_regs[] | svd_if") .. 
                   ', "chans": ' .. luci.sys.exec("echo get_chans[] | svd_if") .. '}')
end
                 