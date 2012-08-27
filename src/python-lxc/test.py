import lxc

t1 = lxc.Container("test")
print("Name set properly: %s" % (t1.name == "test"))
print("Test config loaded properly: %s" % t1.load_config("/etc/lxc/lxc.conf"))
print("Real config loaded properly: %s" % t1.load_config())
print("Test config path: %s" % (t1.config_file_name == "/var/lib/lxc/test/config"))
print("Set config item: %s" % t1.set_config_item("lxc.utsname", "blabla"))
print("Container defined: %s" % (t1.defined))
print("Started properly: %s" % t1.start())
print("Container running: %s" % t1.wait("RUNNING"))
print("Container state: %s" % t1.state)
print("Container running: %s" % t1.running)
print("Container init process: %s" % t1.init_pid)
print("Freezing: %s" % t1.freeze())
print("Container frozen: %s" % t1.wait("FROZEN"))
print("Container state: %s" % t1.state)
print("Unfreezing: %s" % t1.unfreeze())
print("Container running: %s" % t1.wait("RUNNING"))
print("Container state: %s" % t1.state)
print("Stopped properly: %s" % t1.stop())
print("Container state: %s" % t1.state)

#print("Started properly: %s" % t1.start(useinit=True))
#print("Container running: %s" % t1.wait("RUNNING"))
#print("Container state: %s" % t1.state)
#print("Stopped properly: %s" % t1.stop())
#print("Container state: %s" % t1.state)
