bdrsync
=======

This Linux-specific tool efficiently copies contents of one block device to
another, by reading devices block-by-block and only writing to second device
blocks with non-matching content.

It is more efficient than plain dd(1) if the following conditions are met:

* infrequent writes to the second device do not ruin speed of linear read,
* second device write speed is way slower than read speed.

Otherwise don't bother, and just use dd(1).

Note: this tool does not do any locking or thawing of filesystems, so copy might
not be an exact snapshot of source device at any moment of time. IOW, use it
with caution, during low-activity time, and on journalled filesystems.

Motivation
==========

Implementation of [JWZ backups scheme][1] for laptop with complex structure of
LVM volumes and USB-attached hard drive.

[1]: http://www.jwz.org/doc/backups.html

License and Copyright
=====================

GNU GPLv2+

© 2009-2011 Mikhail Gusarov <dottedmag@dottedmag.net>
