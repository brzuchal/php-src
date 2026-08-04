--TEST--
tuple type declarations: parameters, returns, properties, reflection, variance
--FILE--
<?php

/* tuple is a multi-member kind, so this exercises the type layer -- reflection,
 * variance, canonical member order -- with more than one parameter for the
 * first time. The value layer is covered by literal_tuple.phpt. */

function f(tuple[int, string] $a, ?tuple[int] $b): tuple[int, string, bool] {
    return tuple[int, string, bool]{1, 'x', true};
}

class C {
    public tuple[int, string] $p;
    public ?tuple[vec[int], int] $q;
    public function m(tuple[int, tuple[int, int]] $deep): void {}
}

$rf = new ReflectionFunction('f');
echo $rf->getParameters()[0]->getType(), "\n";
echo $rf->getParameters()[1]->getType(), "\n";
echo $rf->getReturnType(), "\n";
echo (new ReflectionProperty('C', 'p'))->getType(), "\n";
echo (new ReflectionProperty('C', 'q'))->getType(), "\n";
echo (new ReflectionMethod('C', 'm'))->getParameters()[0]->getType(), "\n";

echo "-- kind and member types through reflection --\n";
$t = $rf->getReturnType();
echo $t->getCollectionName(), "\n";
echo count($t->getTypes()), "\n";
echo implode(',', array_map(fn($x) => (string) $x, $t->getTypes())), "\n";

echo "-- variance is invariant: same shape ok, reordered members rejected --\n";
class Base {
    public function ok(): tuple[int, string] { return tuple[int, string]{1, 'a'}; }
    public function bad(): tuple[int, string] { return tuple[int, string]{1, 'a'}; }
}
class Ok extends Base {
    public function ok(): tuple[int, string] { return tuple[int, string]{2, 'b'}; }
}
echo "same-shape override: ok\n";

/* An incompatible override is a fatal compile error, so it runs in its own
 * process. */
$code = 'class Base { public function bad(): tuple[int, string] {} }'
      . 'class BadChild extends Base { public function bad(): tuple[string, int] {} }';
$out = trim((string) shell_exec(getenv('TEST_PHP_EXECUTABLE_ESCAPED')
    . ' -n -r ' . escapeshellarg($code) . ' 2>&1'));
echo "reordered override: ", trim(explode("\n", $out)[0]), "\n";

echo "-- arity is variable (>= 1) for a tuple type --\n";
function g(tuple[int] $one, tuple[int, int, int, int] $four): void {}
echo "one- and four-member tuple params accepted\n";

?>
--EXPECT--
tuple[int,string]
?tuple[int]
tuple[int,string,bool]
tuple[int,string]
?tuple[vec[int],int]
tuple[int,tuple[int,int]]
-- kind and member types through reflection --
tuple
3
int,string,bool
-- variance is invariant: same shape ok, reordered members rejected --
same-shape override: ok
reordered override: Fatal error: Declaration of BadChild::bad(): tuple[string,int] must be compatible with Base::bad(): tuple[int,string] in Command line code on line 1
-- arity is variable (>= 1) for a tuple type --
one- and four-member tuple params accepted
