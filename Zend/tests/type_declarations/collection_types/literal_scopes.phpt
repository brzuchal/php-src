--TEST--
collection literals: inside functions, methods, closures and duplicated closures
--EXTENSIONS--
zend_test
--FILE--
<?php

function plain(): int {
    $v = vec[int]{1, 2, 3};
    return zend_test_vec_count($v);
}
var_dump(plain());

class C {
    public vec[int] $prop;

    public function m(): vec[int] {
        return vec[int]{4, 5};
    }

    public static function s(): vec[string] {
        return vec[string]{'s'};
    }
}

$c = new C();
var_dump(zend_test_vec_count($c->m()));
var_dump(zend_test_vec_get(C::s(), 0));

echo "-- assigning a literal to a declared property --\n";
$c->prop = vec[int]{6};
var_dump(zend_test_vec_count($c->prop));
try {
    $c->prop = vec[string]{'no'};
} catch (TypeError $ex) {
    echo $ex->getMessage(), "\n";
}

echo "-- closures --\n";
$closure = function (): vec[int] { return vec[int]{7, 8}; };
var_dump(zend_test_vec_count($closure()));

$arrow = fn(int $n) => vec[int]{$n, $n + 1};
var_dump(zend_test_vec_get($arrow(10), 1));

$static = static function () { return vec[int]{11}; };
var_dump(zend_test_vec_get($static(), 0));

echo "-- a closure that captures --\n";
$captured = 12;
$capturing = function () use ($captured) { return vec[int]{$captured}; };
var_dump(zend_test_vec_get($capturing(), 0));

echo "-- duplicated closures share the compiled descriptor --\n";
/* Closure duplication copies the op_array shallowly and shares the descriptor
 * table. Every copy must build the same type, and destroying the copies must
 * not free a table the original still points at. */
$bound = Closure::bind($closure, null, null);
$rebound = $closure->bindTo(null, null);
$copies = [];
for ($i = 0; $i < 50; $i++) {
    $copies[] = $closure->bindTo(null, null);
}
$ids = [zend_test_vec_type_id($closure())];
$ids[] = zend_test_vec_type_id($bound());
$ids[] = zend_test_vec_type_id($rebound());
foreach ($copies as $copy) {
    $ids[] = zend_test_vec_type_id($copy());
}
var_dump(count(array_unique($ids)) === 1);
unset($copies, $bound, $rebound);
var_dump(zend_test_vec_count($closure()));

echo "-- a method inherited by a subclass --\n";
class D extends C {}
var_dump(zend_test_vec_count((new D())->m()));

echo "-- a trait method, which is copied into each user --\n";
trait T {
    public function t(): vec[int] { return vec[int]{13}; }
}
class E { use T; }
class F { use T; }
var_dump(zend_test_vec_get((new E())->t(), 0), zend_test_vec_get((new F())->t(), 0));
var_dump(zend_test_vec_type_id((new E())->t()) === zend_test_vec_type_id((new F())->t()));

echo "-- a generator --\n";
function gen() { yield vec[int]{14}; }
foreach (gen() as $y) {
    var_dump(zend_test_vec_get($y, 0));
}

echo "-- eval'd code --\n";
$evaled = eval('return vec[int]{15};');
var_dump(zend_test_vec_get($evaled, 0));

?>
--EXPECT--
int(3)
int(2)
string(1) "s"
-- assigning a literal to a declared property --
int(1)
Cannot assign vec[string] to property C::$prop of type vec[int]
-- closures --
int(2)
int(11)
int(11)
-- a closure that captures --
int(12)
-- duplicated closures share the compiled descriptor --
bool(true)
int(2)
-- a method inherited by a subclass --
int(2)
-- a trait method, which is copied into each user --
int(13)
int(13)
bool(true)
-- a generator --
int(14)
-- eval'd code --
int(15)
