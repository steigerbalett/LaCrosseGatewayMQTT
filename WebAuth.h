#ifndef _WEBAUTH_h
#define _WEBAUTH_h

// Login check for handlers outside WebFrontend (OTA uploads, add-on flashing).
// true if no frontend password is set or the request carries the session cookie,
// and the request does not come from a foreign web page (Origin header).
bool WebAuthAllowed();

#endif
