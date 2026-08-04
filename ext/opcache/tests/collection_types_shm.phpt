--TEST--
OPcache: collection type declarations survive SHM persistence and reuse
--INI--
opcache.enable=1
opcache.enable_cli=1
--EXTENSIONS--
opcache
--FILE--
<?php
$file = __DIR__ . '/collection_types_shm.inc';
// First compile persists the script (and its collection descriptors) into SHM.
// opcache_compile_file() persists the script and early-binds its declarations.
var_dump(opcache_compile_file($file));

$rf = new ReflectionFunction('oc_f');
echo $rf->getParameters()[0]->getType(), "\n";
echo $rf->getParameters()[1]->getType(), "\n";
echo $rf->getReturnType(), "\n";
echo (new ReflectionProperty('OcC', 'p'))->getType(), "\n";
echo (new ReflectionProperty('OcC', 'q'))->getType(), "\n";
echo (new ReflectionMethod('OcC', 'm'))->getParameters()[0]->getType(), "\n";
$t = $rf->getReturnType();
echo $t->getCollectionName(), " ", get_class($t->getTypes()[0]), "\n";
?>
--EXPECT--
bool(true)
vec[int]
?vec[string]
vec[vec[int]]
vec[int]
?vec[vec[string]]
vec[vec[vec[int]]]
vec ReflectionCollectionType
