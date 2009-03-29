# A buildsystem plugin for handling Perl Build based projects.
#
# Copyright: © 2008 Joey Hess
#            © 2008 Modestas Vainius
# License: GPL-2+

package Debian::Debhelper::Buildsystems::Perl_Build;

use strict;
use Debian::Debhelper::Dh_Lib;
use Debian::Debhelper::Buildsystem;
use base 'Debian::Debhelper::Buildsystem';

sub PLUGIN_PRIORITY { 10; }

sub is_plugin_applicable {
	return -e "Build.PL";
}

sub new {
	my $cls = shift;
	return bless( {}, $cls );
}

sub configure {
	$ENV{PERL_MM_USE_DEFAULT}=1; # Module::Build can also use this.
	doit("perl", "Build.PL", "installdirs=vendor", @{$dh{U_PARAMS}});
}

sub build {
	doit("perl", "Build", @{$dh{U_PARAMS}});
}

sub test {
	doit(qw/perl Build test/, @{$dh{U_PARAMS}});
}

sub install {
	my ($self, $destdir) = @_;

	doit("perl", "Build", "install", "destdir=$destdir",
		"create_packlist=0", @{$dh{U_PARAMS}});
}

sub clean {
	doit("perl", "Build", "--allow_mb_mismatch", 1, "distclean", @{$dh{U_PARAMS}});
}

1;
