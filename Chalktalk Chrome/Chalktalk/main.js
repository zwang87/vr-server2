var BIND_IP = "127.0.0.1";
var BIND_PORT = 0;

/*

var webview = document.getElementsByTagName("webview")[0];
chrome.sockets.udp.create(function(createInfo) {
chrome.sockets.udp.bind(createInfo.socketId, "127.0.0.1", 0, function(result) {
	webview.addEventListener('consolemessage', function(e) {
			chrome.sockets.udp.send(createInfo.socketId, new TextEncoder("utf-8").encode(e.message).buffer, "127.0.0.1", 1615, function(info) {});
	});
});
});

*/