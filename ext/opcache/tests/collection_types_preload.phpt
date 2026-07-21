--TEST--
OPcache: collection type declarations survive preloading
--INI--
opcache.enable=1
opcache.enable_cli=1
opcache.preload={PWD}/collection_types_preload.inc
--EXTENSIONS--
opcache
--SKIPIF--
<?php
if (PHP_OS_FAMILY == 'Windows') die('skip Preloading is not supported on Windows');
?>
--FILE--
<?php
// Declared only by the preloaded file: reaching them proves the preloaded
// (permanently stored) collection descriptors survived.
$rf = new ReflectionFunction('preloaded_f');
echo $rf->getParameters()[0]->getType(), "\n";
echo $rf->getReturnType(), "\n";
echo (new ReflectionProperty('PreloadedC', 'deep'))->getType(), "\n";
echo (new ReflectionMethod('PreloadedC', 'm'))->getParameters()[0]->getType(), "\n";
$t = $rf->getReturnType();
echo $t->getCollectionName(), " ", get_class($t->getTypes()[0]), "\n";
?>
--EXPECT--
vec[int]
?vec[vec[string]]
vec[vec[int]]
?vec[string]
vec ReflectionCollectionType
