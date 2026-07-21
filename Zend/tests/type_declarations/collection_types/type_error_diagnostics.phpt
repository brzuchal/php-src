--TEST--
collection types: type errors name the full runtime type, not just "collection"
--EXTENSIONS--
zend_test
--FILE--
<?php
zend_test_make_vec([1, 2], 'int', $ints);

/* argument */
function arg(vec[string] $v): void {}
try { arg($ints); } catch (TypeError $e) { echo $e->getMessage(), "\n"; }

/* return */
function ret(vec[int] $v): vec[string] { return $v; }
try { ret($ints); } catch (TypeError $e) { echo $e->getMessage(), "\n"; }

/* property assignment */
class C { public vec[string] $p; }
$c = new C();
try { $c->p = $ints; } catch (TypeError $e) { echo $e->getMessage(), "\n"; }

/* __get() returning a value incompatible with an unset typed property */
class G {
    public vec[string] $p;
    public function __get($n) { global $ints; return $ints; }
}
$g = new G();
unset($g->p);
try { $g->p; } catch (TypeError $e) { echo $e->getMessage(), "\n"; }

/* non-collection diagnostics must be unchanged */
try { arg(1); } catch (TypeError $e) { echo $e->getMessage(), "\n"; }
function plain(int $x): void {}
try { plain([]); } catch (TypeError $e) { echo $e->getMessage(), "\n"; }
class P { public int $q = 0; }
$p = new P();
try { $p->q = []; } catch (TypeError $e) { echo $e->getMessage(), "\n"; }
?>
--EXPECTF--
arg(): Argument #1 ($v) must be of type vec[string], vec[int] given, called in %s on line %d
ret(): Return value must be of type vec[string], vec[int] returned
Cannot assign vec[int] to property C::$p of type vec[string]
Value of type vec[int] returned from G::__get() must be compatible with unset property G::$p of type vec[string]
arg(): Argument #1 ($v) must be of type vec[string], int given, called in %s on line %d
plain(): Argument #1 ($x) must be of type int, array given, called in %s on line %d
Cannot assign array to property P::$q of type int
