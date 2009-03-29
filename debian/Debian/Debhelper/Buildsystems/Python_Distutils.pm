# A buildsystem plugin for handling Python Distutils based projects
#
# Copyright: © 2008 Joey Hess
#            © 2008 Modestas Vainius
# License: GPL-2+

package Debian::Debhelper::Buildsystems::Python_Distutils;

use strict;
use Debian::Debhelper::Dh_Lib;
use Debian::Debhelper::Buildsystem;
use base 'Debian::Debhelper::Buildsystem';

sub PLUGIN_PRIORITY { 10; }

sub is_plugin_applicable {
	return -e "setup.py";
}

sub new {
	my $cls = shift;
	return bless( {}, $cls );
}

sub configure {
	# Do nothing
	1;
}

sub build {
	doit("python", "setup.py", "build", @{$dh{U_PARAMS}});
}

sub test {
	1;
}

sub install {
	my ($self, $destdir) = @_;

	doit("python", "setup.py", "install", 
		"--root=$destdir",
		"--no-compile", "-O0",
		@{$dh{U_PARAMS}});
}

sub clean {
	doit("python", "setup.py", "clean", "-a", @{$dh{U_PARAMS}});
	# The setup.py might import files, leading to python creating pyc
	# files.
	doit('find', '.', '-name', '*.pyc', '-exec', 'rm', '{}', ';');
}

1;
