TEMPLATE = subdirs

smtp.subdir = SMTPEmail

daemon.subdir = accounts-daemon
daemon.depends = smtp

SUBDIRS += smtp \
    daemon

