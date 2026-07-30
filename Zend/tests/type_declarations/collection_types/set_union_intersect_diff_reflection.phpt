--TEST--
set: union()/intersect()/diff() Reflection — set[] $other, set[] return, receiver-isolated
--FILE--
<?php
$a = set[int]{1, 2};
foreach (['union', 'intersect', 'diff'] as $name) {
    $f = $a->$name(...);
    $r = new ReflectionFunction($f);
    echo "$name: return=", (string)$r->getReturnType(),
         " required=", $r->getNumberOfRequiredParameters(),
         " param0=", $r->getParameters()[0]->getName(),
         ":", (string)$r->getParameters()[0]->getType(), "\n";
    // receiver isolation: no bound $this, no leaked static variables
    var_dump($r->getClosureThis() === null);
    var_dump($r->getStaticVariables() === []);
}
?>
--EXPECT--
union: return=set[] required=1 param0=other:set[]
bool(true)
bool(true)
intersect: return=set[] required=1 param0=other:set[]
bool(true)
bool(true)
diff: return=set[] required=1 param0=other:set[]
bool(true)
bool(true)
