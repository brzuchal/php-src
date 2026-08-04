--TEST--
Bare collection-kind types: Reflection (ReflectionCollectionType, getTypes() === [])
--FILE--
<?php
$rf = new ReflectionFunction(function (vec[] $a, ?set[] $b, tuple[] $c): ?vec[] {});
foreach ($rf->getParameters() as $p) {
    $t = $p->getType();
    printf("%s: %s name=%s empty=%s str=%s nullable=%s\n",
        $p->getName(), get_class($t), $t->getCollectionName(),
        var_export($t->getTypes() === [], true), (string) $t,
        var_export($t->allowsNull(), true));
}
$rt = $rf->getReturnType();
printf("ret: str=%s nullable=%s\n", (string) $rt, var_export($rt->allowsNull(), true));

// A concrete descriptor still reflects its members (unchanged)
$t2 = (new ReflectionFunction(function (vec[int] $x): void {}))->getParameters()[0]->getType();
printf("concrete: str=%s empty=%s members=%d\n",
    (string) $t2, var_export($t2->getTypes() === [], true), count($t2->getTypes()));
?>
--EXPECT--
a: ReflectionCollectionType name=vec empty=true str=vec[] nullable=false
b: ReflectionCollectionType name=set empty=true str=?set[] nullable=true
c: ReflectionCollectionType name=tuple empty=true str=tuple[] nullable=false
ret: str=?vec[] nullable=true
concrete: str=vec[int] empty=false members=1
