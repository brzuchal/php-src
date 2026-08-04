--TEST--
Collection types: nullable concrete declarations accept structurally matching values
--FILE--
<?php

function take_vec(?vec[int] $v): string { return $v === null ? 'null' : 'vec'; }
function take_set(?set[string] $s): string { return $s === null ? 'null' : 'set'; }
function take_tuple(?tuple[int,string] $t): string { return $t === null ? 'null' : 'tuple'; }

function give_vec(bool $null): ?vec[int] { return $null ? null : vec[int]{7}; }
function give_set(bool $null): ?set[string] { return $null ? null : set[string]{'a'}; }
function give_tuple(bool $null): ?tuple[int,string] { return $null ? null : tuple[int,string]{1, 'x'}; }

function bad_return(): ?vec[int] { return vec[string]{'a'}; }

class Holder {
    public ?vec[int] $vec = null;
    public ?set[string] $set = null;
    public ?tuple[int,string] $tuple = null;
}

echo "-- parameters accept matching values and null --\n";
var_dump(take_vec(vec[int]{1, 2}));
var_dump(take_vec(null));
var_dump(take_set(set[string]{'a', 'b'}));
var_dump(take_set(null));
var_dump(take_tuple(tuple[int,string]{1, 'x'}));
var_dump(take_tuple(null));

echo "-- returns accept matching values and null --\n";
var_dump(give_vec(false));
var_dump(give_vec(true));
var_dump(give_set(false) !== null);
var_dump(give_set(true));
var_dump(give_tuple(false) !== null);
var_dump(give_tuple(true));

echo "-- properties accept matching values and null --\n";
$h = new Holder();
$h->vec = vec[int]{1};
$h->set = set[string]{'s'};
$h->tuple = tuple[int,string]{2, 'y'};
var_dump($h->vec !== null, $h->set !== null, $h->tuple !== null);
$h->vec = null;
$h->set = null;
$h->tuple = null;
var_dump($h->vec, $h->set, $h->tuple);

echo "-- mismatches still reject --\n";
try {
    take_vec(vec[string]{'a'});
} catch (TypeError $e) {
    echo $e->getMessage(), "\n";
}
try {
    take_vec(set[int]{1});
} catch (TypeError $e) {
    echo $e->getMessage(), "\n";
}
try {
    bad_return();
} catch (TypeError $e) {
    echo $e->getMessage(), "\n";
}
try {
    $h->vec = vec[string]{'a'};
} catch (TypeError $e) {
    echo $e->getMessage(), "\n";
}

?>
--EXPECTF--
-- parameters accept matching values and null --
string(3) "vec"
string(4) "null"
string(3) "set"
string(4) "null"
string(5) "tuple"
string(4) "null"
-- returns accept matching values and null --
vec[int](1) {
  [0]=>
  int(7)
}
NULL
bool(true)
NULL
bool(true)
NULL
-- properties accept matching values and null --
bool(true)
bool(true)
bool(true)
NULL
NULL
NULL
-- mismatches still reject --
take_vec(): Argument #1 ($v) must be of type ?vec[int], vec[string] given, called in %s on line %d
take_vec(): Argument #1 ($v) must be of type ?vec[int], set[int] given, called in %s on line %d
bad_return(): Return value must be of type ?vec[int], vec[string] returned
Cannot assign vec[string] to property Holder::$vec of type ?vec[int]
