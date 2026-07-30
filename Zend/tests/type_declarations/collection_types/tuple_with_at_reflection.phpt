--TEST--
tuple: withAt() Reflection — int $index / mixed $value, tuple[] return, receiver-isolated
--FILE--
<?php
$t = tuple[int, string]{1, "a"};
$f = $t->withAt(...);
$r = new ReflectionFunction($f);

echo "return=", (string)$r->getReturnType(),
     " required=", $r->getNumberOfRequiredParameters();
foreach ($r->getParameters() as $p) {
    echo " | ", $p->getName(), ":", (string)$p->getType();
}
echo "\n";

// receiver isolation: no bound $this, no leaked static variables
var_dump($r->getClosureThis() === null);
var_dump($r->getStaticVariables() === []);
?>
--EXPECT--
return=tuple[] required=2 | index:int | value:mixed
bool(true)
bool(true)
