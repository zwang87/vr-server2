var webview = document.getElementsByTagName("webview")[0];
webview.addEventListener('consolemessage', function(e) {
  chrome.sockets.udp.create(function(createInfo) {
		chrome.sockets.udp.bind(createInfo.socketId, "127.0.0.1", 0, function(result) {
			chrome.sockets.udp.send(createInfo.socketId, new TextEncoder("utf-8").encode(e.message).buffer, "127.0.0.1", 1615, function(oInfo) {
			});
		});
	});
});