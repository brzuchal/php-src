<?php
/* Pivot results/raw.csv into the markdown tables used by summary.md. */
$rows = [];
$f = fopen(__DIR__ . '/results/raw.csv', 'r');
$hdr = fgetcsv($f, 0, ',', '"', '\\');
while (($r = fgetcsv($f, 0, ',', '"', '\\')) !== false) {
    $rows[] = array_combine($hdr, $r);
}
fclose($f);

function sel(array $rows, array $where): array {
    return array_values(array_filter($rows, function ($r) use ($where) {
        foreach ($where as $k => $v) if ((string) $r[$k] !== (string) $v) return false;
        return true;
    }));
}
function one(array $rows, array $where): ?array {
    $s = sel($rows, $where);
    return $s ? $s[0] : null;
}
function fmt($v): string {
    if ($v === null || $v === '') return '-';
    $v = (float) $v;
    if ($v >= 1e6) return sprintf('%.2fms', $v / 1e6);
    if ($v >= 1e3) return sprintf('%.1fus', $v / 1e3);
    return sprintf('%.1fns', $v);
}
function bytes($v): string {
    $v = (float) $v;
    if ($v >= 1 << 20) return sprintf('%.1fMB', $v / (1 << 20));
    if ($v >= 1 << 10) return sprintf('%.1fKB', $v / (1 << 10));
    return sprintf('%.0fB', $v);
}

echo "## Q1: one append onto a 100k flat base (original retained, per elem)\n\n";
echo "| elem | repr | median/append | zval copies/op | allocs/op | bytes/op |\n|---|---|---|---|---|---|\n";
foreach (['int', 'string'] as $el) {
    foreach ([['A', 0], ['B', 0], ['C', 16], ['C', 64], ['C', 128]] as [$r, $ch]) {
        $row = one($rows, ['workload' => 'base_append', 'repr' => $r, 'chunk' => $ch,
            'elem' => $el, 'base' => 100000, 'appended' => 1]);
        if ($row) printf("| %s | %s%s | %s | %.1f | %.2f | %s |\n", $el, $r,
            $ch ? " c$ch" : '', fmt($row['median_ns']), $row['zcopies_op'],
            $row['allocs_op'], bytes($row['bytes_op']));
    }
}

echo "\n## Q1b: base scaling, appended=1, int (median ns/append)\n\n";
echo "| base | A | B | C c64 | A/C ratio |\n|---|---|---|---|---|\n";
foreach ([100, 1000, 10000, 100000] as $b) {
    $a = one($rows, ['workload' => 'base_append', 'repr' => 'A', 'elem' => 'int', 'base' => $b, 'appended' => 1]);
    $bb = one($rows, ['workload' => 'base_append', 'repr' => 'B', 'elem' => 'int', 'base' => $b, 'appended' => 1]);
    $c = one($rows, ['workload' => 'base_append', 'repr' => 'C', 'chunk' => 64, 'elem' => 'int', 'base' => $b, 'appended' => 1]);
    printf("| %d | %s | %s | %s | %.0fx |\n", $b, fmt($a['median_ns']), fmt($bb['median_ns']),
        fmt($c['median_ns']), $a['median_ns'] / max(0.001, $c['median_ns']));
}

echo "\n## Q1c: amortization — base=100k int, varying appended (ns/append)\n\n";
echo "| appended | A | B | C c16 | C c64 | C c128 |\n|---|---|---|---|---|---|\n";
foreach ([1, 4, 16, 32, 64, 1024] as $k) {
    $cells = [];
    foreach ([['A', 0], ['B', 0], ['C', 16], ['C', 64], ['C', 128]] as [$r, $ch]) {
        $row = one($rows, ['workload' => 'base_append', 'repr' => $r, 'chunk' => $ch,
            'elem' => 'int', 'base' => 100000, 'appended' => $k]);
        $cells[] = $row ? fmt($row['median_ns']) : '-';
    }
    printf("| %d | %s |\n", $k, implode(' | ', $cells));
}

echo "\n## Q2: branch cost by chunk and receiver position (int)\n\n";
echo "| chunk | lv_pos | lv | median | p95 | zcopies/op (bytes) | blk_addref/op | allocs/op |\n|---|---|---|---|---|---|---|---|\n";
foreach ([16, 32, 64, 128] as $ch) {
    foreach (['half', 'worst', 'boundary'] as $pos) {
        $row = one($rows, ['workload' => 'branch', 'repr' => 'C', 'chunk' => $ch,
            'elem' => 'int', 'lv_pos' => $pos]);
        if (!$row) continue;
        $ex = json_decode($row['extra'] ?: '{}', true);
        printf("| %d | %s | %d | %s | %s | %.0f (%s) | %.1f | %.1f |\n", $ch, $pos,
            $ex['lv_at_branch'] ?? -1, fmt($row['median_ns']), fmt($row['p95_ns']),
            $row['zcopies_op'], bytes($row['zcopies_op'] * 16), $row['blk_addref_op'],
            $row['allocs_op']);
    }
}
echo "\nA/B comparison rows (append to shared older version = full copy):\n\n";
echo "| repr | chunk-context | elem | median | zcopies/op |\n|---|---|---|---|---|\n";
foreach (['int', 'string'] as $el) {
    foreach ([['A', 4], ['B', 4], ['A', 7], ['B', 7]] as [$r, $sh]) {
        $rr = sel($rows, ['workload' => 'branch', 'repr' => $r, 'elem' => $el, 'lv_pos' => 'half']);
        foreach ($rr as $row) {
            printf("| %s | shift %d group | %s | %s | %.0f |\n", $r, $sh, $el,
                fmt($row['median_ns']), $row['zcopies_op']);
        }
        break; /* rows not distinguished by shift in csv for A/B; take all once */
    }
}

echo "\n## Q3: indexed read, base=10000 + 8 blocks (int), ns/read\n\n";
echo "| repr/chunk | base region | full blocks | last block | mixed |\n|---|---|---|---|---|\n";
foreach ([['A', 0], ['B', 0], ['C', 16], ['C', 32], ['C', 64], ['C', 128]] as [$r, $ch]) {
    $cells = [];
    foreach (['base', 'full', 'last', 'mixed'] as $rg) {
        $row = one($rows, ['workload' => 'read', 'repr' => $r, 'chunk' => $ch, 'region' => $rg]);
        $cells[] = $row ? sprintf('%.2f', $row['median_ns']) : '-';
    }
    printf("| %s%s | %s |\n", $r, $ch ? " c$ch" : '', implode(' | ', $cells));
}

echo "\n## Q4: foreach ns/element, base=10000 (int)\n\n";
echo "| blocks | C c16 | C c32 | C c64 | C c128 | A same-total (c64 sizes) |\n|---|---|---|---|---|---|\n";
foreach ([0, 1, 8, 64, 1024] as $bl) {
    $cells = [];
    foreach ([16, 32, 64, 128] as $ch) {
        $row = one($rows, ['workload' => 'foreach', 'repr' => 'C', 'chunk' => $ch,
            'elem' => 'int', 'blocks' => $bl]);
        $cells[] = $row ? sprintf('%.3f', $row['median_ns']) : '-';
    }
    $total = 10000 + $bl * 64;
    $a = one($rows, ['workload' => 'foreach', 'repr' => 'A', 'elem' => 'int', 'base' => $total]);
    printf("| %d | %s | %s |\n", $bl, implode(' | ', $cells),
        $a ? sprintf('%.3f', $a['median_ns']) : '-');
}
echo "\nA flat baselines by total: ";
foreach (sel($rows, ['workload' => 'foreach', 'repr' => 'A', 'elem' => 'int']) as $a) {
    printf("%d=%.3f ", $a['base'], $a['median_ns']);
}
echo "\n";

echo "\n## Q5: invisible retention (string elems, base=1000, tip=40 blocks, snapshot at ~10.5 blocks)\n\n";
echo "| chunk | snap_at | visible | invisible slots | bound | blocks live | spine arr bytes | live bytes @snap | live bytes @tip |\n|---|---|---|---|---|---|---|---|---|\n";
foreach (sel($rows, ['workload' => 'invisible']) as $row) {
    $ex = json_decode($row['extra'] ?: '{}', true);
    printf("| %d | %d | %.0f | %.0f | %.0f | %.0f | %s | %s | %s |\n",
        $row['chunk'], $row['n'], $ex['visible_count'], $ex['invisible_slots'],
        $ex['invisible_slot_bound'], $ex['blocks_live_snap'],
        bytes($ex['spine_arr_bytes_snap']), bytes($ex['live_bytes_snap']),
        bytes($ex['live_bytes_tip']));
}

echo "\n## Q6/Q8: version materialization + destroy vs visible blocks (boundary branch, int)\n\n";
echo "| vb | c16 create | c16 destroy | c64 create | c64 destroy | c128 create | c128 destroy |\n|---|---|---|---|---|---|---|\n";
foreach ([1, 8, 64, 1024] as $vb) {
    $cells = [];
    foreach ([16, 64, 128] as $ch) {
        $row = one($rows, ['workload' => 'vcreate', 'chunk' => $ch, 'blocks' => $vb]);
        $cells[] = $row ? fmt($row['median_ns']) : '-';
        $cells[] = $row ? fmt($row['median2_ns']) : '-';
    }
    printf("| %d | %s |\n", $vb, implode(' | ', $cells));
}

echo "\n## Q7: linear append, intermediates dead (ns/append)\n\n";
echo "| elem | n | A | B | C c16 | C c32 | C c64 | C c128 |\n|---|---|---|---|---|---|---|---|\n";
foreach (['int', 'string', 'object', 'array', 'nested'] as $el) {
    foreach ([1024, 4096, 65536] as $n) {
        $cells = [];
        foreach ([['A', 0], ['B', 0], ['C', 16], ['C', 32], ['C', 64], ['C', 128]] as [$r, $ch]) {
            $row = one($rows, ['workload' => 'linear', 'repr' => $r, 'chunk' => $ch,
                'elem' => $el, 'n' => $n, 'retained' => 0]);
            $cells[] = $row ? fmt($row['median_ns']) : '-';
        }
        printf("| %s | %d | %s |\n", $el, $n, implode(' | ', $cells));
    }
}

echo "\n## W2: linear append, all versions retained (ns/append)\n\n";
echo "| elem | n | A | B | C c16 | C c32 | C c64 | C c128 |\n|---|---|---|---|---|---|---|---|\n";
foreach (['int', 'string', 'object'] as $el) {
    foreach ([1024, 4096, 16384, 65536] as $n) {
        $cells = [];
        $any = false;
        foreach ([['A', 0], ['B', 0], ['C', 16], ['C', 32], ['C', 64], ['C', 128]] as [$r, $ch]) {
            $row = one($rows, ['workload' => 'linear', 'repr' => $r, 'chunk' => $ch,
                'elem' => $el, 'n' => $n, 'retained' => 1]);
            $cells[] = $row ? fmt($row['median_ns']) : '-';
            $any = $any || $row;
        }
        if ($any) printf("| %s | %d | %s |\n", $el, $n, implode(' | ', $cells));
    }
}
echo "\nRetained memory at build (mem_build - mem_start), int:\n\n";
echo "| n | A | B | C c16 | C c64 | C c128 |\n|---|---|---|---|---|---|\n";
foreach ([1024, 4096, 16384, 65536] as $n) {
    $cells = [];
    $any = false;
    foreach ([['A', 0], ['B', 0], ['C', 16], ['C', 64], ['C', 128]] as [$r, $ch]) {
        $row = one($rows, ['workload' => 'linear', 'repr' => $r, 'chunk' => $ch,
            'elem' => 'int', 'n' => $n, 'retained' => 1]);
        $cells[] = $row ? bytes($row['mem_build'] - $row['mem_start']) : '-';
        $any = $any || $row;
    }
    if ($any) printf("| %d | %s |\n", $n, implode(' | ', $cells));
}

echo "\n## W8: destruction (ns/element; retained = ns per element incl. version chain)\n\n";
echo "| mode | elem | A | B | C c16 | C c128 |\n|---|---|---|---|---|---|\n";
foreach ([0, 1] as $ret) {
    foreach (['int', 'string', 'object', 'array', 'nested'] as $el) {
        $cells = [];
        $any = false;
        foreach ([['A', 0], ['B', 0], ['C', 16], ['C', 128]] as [$r, $ch]) {
            $row = one($rows, ['workload' => 'destroy', 'repr' => $r, 'chunk' => $ch,
                'elem' => $el, 'retained' => $ret]);
            $cells[] = $row ? fmt($row['median_ns']) : '-';
            $any = $any || $row;
        }
        if ($any) printf("| %s | %s | %s |\n", $ret ? 'chain' : 'single', $el, implode(' | ', $cells));
    }
}

echo "\n## W10: forks (base=10000, per=8 appends per fork, int; ns/fork create, destroy)\n\n";
echo "| forks | C create | C destroy | C live bytes | A create | A live bytes |\n|---|---|---|---|---|---|\n";
foreach ([1, 4, 16, 64, 256] as $fk) {
    $c = one($rows, ['workload' => 'forks', 'repr' => 'C', 'forks' => $fk]);
    $a = one($rows, ['workload' => 'forks', 'repr' => 'A', 'forks' => $fk]);
    $cx = $c ? json_decode($c['extra'] ?: '{}', true) : [];
    $ax = $a ? json_decode($a['extra'] ?: '{}', true) : [];
    printf("| %d | %s | %s | %s | %s | %s |\n", $fk,
        $c ? fmt($c['median_ns']) : '-', $c ? fmt($c['median2_ns']) : '-',
        $c ? bytes($cx['live_bytes_all_forks'] ?? 0) : '-',
        $a ? fmt($a['median_ns']) : '-',
        $a ? bytes($ax['live_bytes_all_forks'] ?? 0) : '-');
}
echo "\n## W10b: branch depth (chained branches, int, c64; ns/level)\n\n";
echo "| depth | keep_all | median/level | live bytes at depth |\n|---|---|---|---|\n";
foreach (sel($rows, ['workload' => 'depth']) as $row) {
    $ex = json_decode($row['extra'] ?: '{}', true);
    printf("| %d | %s | %s | %s |\n", $row['depth'], $row['keep_all'],
        fmt($row['median_ns']), bytes($ex['live_bytes_depth'] ?? 0));
}

echo "\n## Sanity: peak memory, linear dead n=65536 int\n\n";
foreach ([['B', 0], ['C', 16], ['C', 128]] as [$r, $ch]) {
    $row = one($rows, ['workload' => 'linear', 'repr' => $r, 'chunk' => $ch,
        'elem' => 'int', 'n' => 65536, 'retained' => 0]);
    if ($row) printf("%s%s: peak-start=%s build-start=%s\n", $r, $ch ? " c$ch" : '',
        bytes($row['mem_peak'] - $row['mem_start']),
        bytes($row['mem_build'] - $row['mem_start']));
}
