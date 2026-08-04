--TEST--
collection types: runtime acceptance and rejection
--EXTENSIONS--
zend_test
--FILE--
<?php
function want_int_vec(vec[int] $v): void { echo "accepted\n"; }
function want_str_vec(vec[string] $v): void { echo "accepted\n"; }

zend_test_make_vec([1, 2, 3], 'int', $ints);
want_int_vec($ints);

try { want_str_vec($ints); } catch (TypeError $e) { echo $e->getMessage(), "\n"; }
try { want_int_vec(1); } catch (TypeError $e) { echo $e->getMessage(), "\n"; }
try { want_int_vec([1,2]); } catch (TypeError $e) { echo $e->getMessage(), "\n"; }
try { want_int_vec(null); } catch (TypeError $e) { echo $e->getMessage(), "\n"; }
?>
--EXPECTF--
accepted
want_str_vec(): Argument #1 ($v) must be of type vec[string], vec[int] given, called in %s on line %d
want_int_vec(): Argument #1 ($v) must be of type vec[int], int given, called in %s on line %d
want_int_vec(): Argument #1 ($v) must be of type vec[int], array given, called in %s on line %d
want_int_vec(): Argument #1 ($v) must be of type vec[int], null given, called in %s on line %d
