# This file is part of Cockpit.
#
# Copyright (C) 2013 Red Hat, Inc.
#
# Cockpit is free software; you can redistribute it and/or modify it
# under the terms of the GNU Lesser General Public License as published by
# the Free Software Foundation; either version 2.1 of the License, or
# (at your option) any later version.
#
# Cockpit is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public License
# along with Cockpit; If not, see <http://www.gnu.org/licenses/>.

set -eu

TEST_OS=${TEST_OS:-fedora-20}
TEST_ARCH=${TEST_ARCH:-x86_64}
TEST_RUN=./run

warning()
{
	echo >&2 "$SELF: $@"
}

silent()
{
	"$@" > /dev/null 2> /dev/null
	return $?
}
