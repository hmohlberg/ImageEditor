#
# Copyright 2026 Forschungszentrum Jülich
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#    https://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

#!/usr/bin/perl
use strict;
use warnings;

# >>>
my $program = "../build/ImageEditor --batch";

# get max available batch id
my $dir = 'projects';
die "Directory '$dir' not found.\n" unless ( -d $dir );
my $maxAvailableBatchId = -1;
opendir(my $dh, $dir) or die "Error: Cannot open projects diretory '$dir': $!";
# Readin files 
while ( my $file = readdir($dh) ) {
    if ( $file =~ /^imageeditor_batch(\d+)\.json$/ ) {
        my $current_id = $1;
        if  ( $current_id > $maxAvailableBatchId ) {
            $maxAvailableBatchId = $current_id;
        }
    }
}
closedir($dh);

# select batch id
print "Select batch job [1-".$maxAvailableBatchId."]: ";
my $batchId = <STDIN>;
chomp($batchId);

# run
my $projectfile = "projects/imageeditor_batch".$batchId.".json";
if ( -e $projectfile ) {
 my $outfile = "imageeditor_batch".$batchId.".png";
 system($program." --force --project $projectfile --output ".$outfile);
} else {
 die "Error: Invalid project file '".$projectfile."': $!";
}
