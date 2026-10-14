# Attribution: retired unenrolled duplicate shards

`the old credit check` reported lost path credit when an already-unenrolled duplicate shard was deleted, even though the canonical production function and its explicit member credit were unchanged.

Retirement is a separate result from consolidation. It requires every one of: one configured function; the same unique module, address, size and symbol in both revisions; the same matched owner, and that path is the sole `complete` owner; unchanged explicit `path#symbol` credit; and base-revision proof the deleted shard was already unenrolled. Proof is a delinks entry that names the shard, is not `complete`, and covers that function alone, or no delinks entry at all while the basename is that symbol and a different path is the complete owner. The report stores the shard's historical path author separately from the canonical function author. Those two people need not match. A configured symbol without this proof still fails.

Tests cover distinct and equal authors, a shard filename that is not the symbol, and rejected changes of size, address, symbol, module, uniqueness, matched owner, enrollment, coverage, and member credit.
