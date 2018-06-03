# multipath_kfree

# DISABLE SIRI BEFORE RUNNING 
low effort jb for iOS 11.3.1 by [@jaakerblom](https://twitter.com/jaakerblom)

Sets up kernel RWX with clear API

Tested on iPhone X only

Uses [QiLin](http://newosxbook.com/QiLin/) by Jonathan Levin

Thanks to:
 * Everyone including Stefan Esser

Special thanks to:
 * [@i41nbeer](https://twitter.com/i41nbeer) 
 * [@doadam](https://twitter.com/doadam)
 * Mr. 0xd503201f
 * [@Morpheus______](https://twitter.com/Morpheus______) 

Note about Siri: Siri has the multipath entitlement and seems to be using multipath sockets. The current code does not account for this as it aspects a new page for the multipath socket structs, therefore you either have to disable Siri or change the heap  logic before running. 
