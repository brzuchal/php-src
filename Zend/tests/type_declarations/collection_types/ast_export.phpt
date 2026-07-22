--TEST--
collection types: a collection type is exported correctly by the AST exporter
--INI--
zend.assertions=1
assert.exception=1
--FILE--
<?php

/* assert() renders its argument through the AST exporter, which is the only
 * consumer that has to re-print a type from the AST rather than from a compiled
 * zend_type. The head is not a name node -- the kind lives in `attr` -- so the
 * exporter has to spell it from the kind. */

try { assert((function (vec[int] $x) {}) === null); }
catch (AssertionError $e) { echo $e->getMessage(), "\n"; }

try { assert((function (?vec[string] $x) {}) === null); }
catch (AssertionError $e) { echo $e->getMessage(), "\n"; }

try { assert((function (vec[vec[Foo]] $x) {}) === null); }
catch (AssertionError $e) { echo $e->getMessage(), "\n"; }

try { assert((function ($x): vec[int] { return $x; }) === null); }
catch (AssertionError $e) { echo $e->getMessage(), "\n"; }

?>
--EXPECT--
assert(function (vec[int] $x) {
} === null)
assert(function (?vec[string] $x) {
} === null)
assert(function (vec[vec[Foo]] $x) {
} === null)
assert(function ($x): vec[int] {
    return $x;
} === null)
