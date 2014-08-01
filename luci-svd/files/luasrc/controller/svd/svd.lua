module("luci.controller.svd.svd", package.seeall)

function index()
   page = node("admin", "svd")
   page.target = firstchild()
   page.title  = translate("Voip")
   page.order  = 90
                                  
   entry({"admin","svd","state"}, template("svd/status"),translate("Status"),20) 
   entry({"admin","svd","main"}, cbi("svd/main"),translate("Main"), 30)
   entry({"admin","svd","channels"}, cbi("svd/channels"),translate("Channels"), 40)
   entry({"admin","svd","accounts"}, cbi("svd/accounts"),translate("Accounts"), 50)
   entry({"admin","svd","dialplan"}, cbi("svd/dialplan"),translate("Dialplan"), 60)
   entry({"admin", "svd", "account_status"}, call("svd_get_account_status")).leaf = true
   entry({"admin", "svd", "channels_status"}, call("svd_get_channels_status")).leaf = true
end
     
function svd_get_account_status()
   luci.http.prepare_content("application/json")
   luci.http.write(luci.sys.exec("echo get_regs[] | svd_if"))
end

function svd_get_channels_status()
   luci.http.prepare_content("application/json")
   luci.http.write(luci.sys.exec("echo get_chans[] | svd_if"))
end
                 