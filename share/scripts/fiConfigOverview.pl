#! /usr/bin/perl -w

=head1 NAME

fiConfigOverview - create overview tables of fimex config

=head1 SYNOPSIS

fiConfigOverview -i DIR/FILES -o OUT.html

=head1 DESCRIPTION

fiConfigOverview read a single or a directory of fimex input/output
xml-configuration files and writes an overview-table in html format.

=head2 OPTIONS

=over8

=item -h

help

=item -i

comma-separated list of files or directories

=item -o

output-file

=back

=head1 AUTHOR

Heiko Klein, E<lt>Heiko.Klein@met.noE<gt>

=head1 SEE ALSO

https://wiki.met.no/fimex/

=cut

use 5.6.1;
use strict;
use warnings;
use Pod::Usage qw(pod2usage);
use Getopt::Std qw(getopts);
use File::Find  qw();

our %Args;
our @Args = @ARGV;
$Args{o} = '-';
getopts('hi:o:', \%Args)
    or pod2usage(2);
pod2usage(-exitval => 0,
          -verbose => 2) if $Args{h};
pod2usage(-exitval => 2,
          -msg => "Unkown emission input -i") unless ($Args{i});

File::Find::find({
    wanted => sub {-f $_ && Fimex::Config::parseFile($File::Find::name);},
    follow => 1,
    no_chdir => 1,
}, split (',', $Args{i}));

Fimex::Config::writeHtml($Args{o});


#####################################################################

package Fimex::Config;
use Carp qw(carp);
our %content; # type => id => Fimex::Config::Content object

use constant DEBUG => 0;

use constant NAMESPACES => {
    gr => 'http://www.met.no/schema/fimex/cdmGribReaderConfig',
    ncml => 'http://www.unidata.ucar.edu/namespaces/netcdf/ncml-2.2',
};

use constant IGNORE_APX => [qw(dtd xsd cfg)];

use constant FILETYPES => {
    "gr:cdmGribReaderConfig" => \&parseGribReader,
    'ncml:netcdf' => \&ignore,
    "cdm_felt_config" => \&parseFeltConfig,
        # XINCLUDEs for cdm_felt_config
        "variables" => \&parseFeltConfig,
        'global_attributes' => \&ignore,
        'axes' => \&ignore,
    'cdm_gribwriter_config' => \&parseGribWriter,
    'cdm_ncwriter_config' => \&ignore,
    'cdmQualityConfig' => \&ignore,
    'gribFileIndex' => \&ignore,
    'metgm_config' => \&ignore,
    "wdb_netcdf_config" => \&parseWdbReader,
    'wdb_query' => \&ignore,
};


sub parseFile {
    my ($file) = @_;

    # ignore all .-files or files in .-directories
    return if $file =~ /\/\./;
    foreach my $apx (@{IGNORE_APX()}) {
        if ($file =~ /\Q$apx\E$/) {
            print STDERR "ignore $file $apx\n" if DEBUG;
            return;
        }
    }

    # late loading of external modules
    require XML::LibXML;
    require XML::LibXML::XPathContext;

    eval {
        my $doc = XML::LibXML->load_xml(location => $file);
        my $xpc = XML::LibXML::XPathContext->new($doc);
        while (my ($prefix, $uri) = each %{NAMESPACES()}) {
            $xpc->registerNs($prefix, $uri);
        }
        my $found = 0;
        my @content;
        foreach my $rootPath (keys %{FILETYPES()}) {
            if ($xpc->exists($rootPath)) {
                @content = FILETYPES->{$rootPath}->($file, $xpc, $doc);
                $found++;
                print STDERR "found $rootPath in $file\n" if DEBUG;
                last;
            }
        }
        if ($found) {
            addContent(@content);
        } else {
            print STDERR "unknown xml-file: $file\n";
        }
    }; if ($@) {
        if ($@ =~ /parser error/) {
            print STDERR "invalid xml-file: $file\n";
        } else {
            die $@;
        }
        return;
    }
}

sub addContent {
    my @content = @_;
    foreach my $c (@content) {
        if (exists $content{$c->type}{$c->id}) {
            $content{$c->type}{$c->id}->merge($c);
        } else {
            $content{$c->type}{$c->id} = $c;
        }
    }
}

sub parseGribReader {
    my ($filename, $xpc, $doc) = @_;
    my @content;
    foreach my $node ($xpc->findnodes("/gr:cdmGribReaderConfig/gr:variables/gr:parameter")) {
        my $xml = $node->toString(1);
        my $varName;
        my %attr = map {$_->getName => $_->getValue} $node->attributes;
        $varName = $attr{name};
        my ($longName, $metnoName, $unit, $standardName);
        foreach my $attNode ($xpc->findnodes("gr:attribute", $node)) {
            my %attr = map {$_->getName => $_->getValue} $attNode->attributes;
            if ($attr{name} eq "units") {
                $unit = $attr{value};
            } elsif ($attr{name} eq "long_name") {
                $longName = $attr{value};
            } elsif ($attr{name} eq "standard_name") {
                $standardName = $attr{value};
            }
        }
        foreach my $param ($xpc->findnodes("gr:grib1", $node)) {
            my %attr = map {$_->getName => $_->getValue} $param->attributes;
            my @idTerms = map {exists $attr{$_} ? ($attr{$_}) : ("")} qw(indicatorOfParameter gribTablesVersionNo identificationOfOriginatingGeneratingCentre typeOfLevel);
            my $id = join ",", @idTerms;
            push @content, Fimex::Config::Content::Grib1->new($id, $xml, $filename, $unit, $standardName, $varName, $longName, $metnoName);
        }
        foreach my $param ($xpc->findnodes("gr:grib2", $node)) {
            my %attr = map {$_->getName => $_->getValue} $param->attributes;
            my @idTerms = map {exists $attr{$_} ? ($attr{$_}) : ("")} qw(discipline parameterCategory parameterNumber typeOfLevel);
            my $id = join ",", @idTerms;
            push @content, Fimex::Config::Content::Grib2->new($id, $xml, $filename, $unit, $standardName, $varName, $longName, $metnoName);
        }
    }
    return @content;
}

sub parseGribWriter {
    my ($fileName, $xpc, $doc) = @_;
    return ();
}

sub parseWdbReader {
    my ($fileName, $xpc, $doc) = @_;
    return ();

}

sub parseFeltConfig {
    my ($fileName, $xpc, $doc) = @_;
    return ();
}

sub ignore {
    my ($fileName, $xpc, $doc) = @_;
    print STDERR "ignore $fileName\n" if DEBUG;
    return ();
}


sub writeHtml {
    my ($outFile) = @_;

    open HTML, ">$outFile"
        or die "Cannot write $outFile: $!\n";
    print HTML <<'EOT';
<!DOCTYPE HTML PUBLIC "-//W3C//DTD HTML 4.01 Transitional//EN">
<html>
<head>
<meta http-equiv="content-type" content="text/html; charset=UTF-8">
<meta name="author" content="Heiko Klein, heiko.klein@met.no">
<style type="text/css">
<!--
#main
{
  padding: 0;
  margin: 0;
  border-collapse: collapse;
  border: 1px solid #333;
  font-family: Verdana, Arial, Helvetica, sans-serif;
  font-size: 0.9em;
  color: #000;
  background: #bcd0e4 top left repeat-x;
}

#main caption
{
  caption-side: bottom;
  font-size: 0.9em;
  font-style: italic;
  text-align: right;
  padding: 0.5em 0;
}

#main th, #main td
{
  border: 1px dotted #666;
  padding: 0.5em;
  text-align: left;
}

.hidden
{
    display: none;
}

#main td
{
  background: #ffffff;
}
-->
</style>

<script src="http://code.jquery.com/jquery-1.7.2.min.js" type="text/javascript"></script>
<script src="http://fimex.met.no/js/stupidtable.min.js" type="text/javascript"></script>
<script type="text/javascript">
<!--
"use strict";
EOT
    print HTML 'var fiConf = ', Fimex::JSON::encode(\%content),";\n";
    print HTML <<'EOT';
-->
</script>
<script type="text/javascript">
<!--
"use strict";
$(document).ready(function() {
var cols = ["varName", "standardName", "grib1Id", "grib2Id", "feltId", "longName", "metnoName"];
var type, id, i, j;
var $col, $row;
// col 0 is a index-row to fiConfRows with all information
var fiConfRows = [];
for (type in fiConf) {
    for (id in fiConf[type]) {
        for (i = 0; i < fiConf[type][id]["varName"].length; ++i) {
            $row = $(document.createElement('tr'));
            $("#main").append($row);
            $row.append($(document.createElement('td')).attr({'class': 'hidden'})
                        .append(fiConfRows.length));
            fiConfRows.push(fiConf[type][id]);
            for (j = 0; j < cols.length; ++j) {
                $(document.createElement('td'))
                .append(fiConf[type][id][cols[j]][i])
                .appendTo($row);
            }
        }
    }
}

$("#main").stupidtable();

});
-->
</script>
<title>Fimex Config Overview</title>
</head>
<body>
<h1>Fimex Config Overview</h1>
EOT
    print HTML <<"EOT";
<table id='main'>
<caption>generated by $0 @main::Args</caption>
<thead>
<tr><th class="hidden type-int">No.</th><th class="type-string">variable</th><th class="type-string">standard-name</th><th class="type-int">grib1</th><th class="type-int">grib2</th><th class="type-int">felt-id</th><th class="type-string">long_name</th><th class="type-string">metno-name</th></tr>
</thead>
<tbody></tbody>
</table>
</html>
EOT
    close HTML;
}

########################################################################
# minimum requirements to fill data with
package Fimex::Config::Content;


sub mk_listaccessors_ {
    my ($class, @fields) = @_;
    foreach my $f (@fields) {
        my $func = sub {
            my ($self, $fData) = @_;
            push @{$self->{$f}}, $fData if defined $fData;
            return @{ $self->{$f} };
        };
        my $funcName = $class . '::' . $f;
        no strict 'refs';
        *{$funcName} = $func;
    }
}

BEGIN {
    __PACKAGE__->mk_listaccessors_(qw(unit varName xml fileName longName standardName metnoName grib1Id grib2Id feltId));
    die unless UNIVERSAL::can(__PACKAGE__, 'xml');
}

sub new {
    my ($package, $id, $xml, $filename, $unit, $standardName, $varName, $longName, $metnoName) = @_;
    die "don't use interface, use implementation" if ($package eq __PACKAGE__);
    my $self = {
        id => $id,
        unit => [],
        varName => [],
        longName => [],
        standardName => [],
        metnoName => [],
        grib1Id => [],
        grib2Id => [],
        feltId => [],
        xml => [],
        fileName => [],
    };
    bless $self, $package;
    $self->xml($xml || "");
    $self->unit($unit || "");
    $self->fileName($filename || "");
    $self->standardName($standardName || "");
    $self->varName($varName || "");
    $self->longName($longName || "");
    $self->metnoName($metnoName || "");
    return $self;
}

sub type {
    die "not implemented in base";
}

sub id {
    return $_[0]->{id};
}

=head2 merge(other)

merge information from $other to this object

=cut

sub merge {
    my ($self, $other) = @_;
    if ($self->isa($other)) {
        die "cannot merge different types";
    }
    foreach my $key (%{ $other }) {
        if (ref($self->{$key}) eq 'ARRAY') {
            push @{ $self->{$key} }, @{$other->{$key}};
        }
    }

}

package Fimex::Config::Content::Grib1;
use base qw(Fimex::Config::Content);

sub new {
    my $self = Fimex::Config::Content::new(@_);
    $self->grib1Id($self->id);
    return $self;
}

sub type {
    return "grib1";
}

package Fimex::Config::Content::Grib2;
use base qw(Fimex::Config::Content);

sub new {
    my $self = Fimex::Config::Content::new(@_);
    $self->grib2Id($self->id);
    return $self;
}

sub type {
    return "grib2";
}

#####################################################################
package Fimex::JSON;
use Scalar::Util qw(reftype);
sub encode {
    my ($var) = @_;
    my $ref = reftype $var;
    if (!defined $ref) {
        return encodeScalar($var);
    } elsif ($ref eq 'SCALAR') {
        return encodeScalar($$var);
    } elsif ($ref eq 'HASH') {
        return encodeHashRef($var);
    } elsif ($ref eq 'ARRAY') {
        return encodeArrayRef($var);
    }
}

sub encodeHashRef {
    my ($h) = @_;
    my %out;
    while (my ($key, $val) = each %$h) {
        $out{$key} = encode($val);
    }
    return "{". join(',', map {encodeScalar($_).': '. $out{$_} } keys %out) ."}";
}

sub encodeArrayRef {
    my ($a) = @_;
    my @out;
    foreach my $e (@$a) {
        push @out, encode($e);
    }
    return "[". join(',', @out) . "]";
}

sub encodeScalar {
    my ($s) = @_;

    # escape
    $s =~ s:\\:\\\\:g;
    $s =~ s:/:\\/:g;
    $s =~ s:":\\":g;
    $s =~ s:\r:\\r:g;
    $s =~ s:\n:\\n:g;
    $s =~ s:\t:\\t:g;
    return '"'. $s . '"';
}