package com.yuminaga.motonav;

import java.util.LinkedHashSet;

/** An Android launch request is a one-shot command, not a description of the resumed page. */
public final class DestinationRequestLedger {
    private final LinkedHashSet<String> handled = new LinkedHashSet<>();
    public synchronized boolean claim(String id) {
        if (id == null || id.isEmpty() || !handled.add(id)) return false;
        while (handled.size() > 128) handled.remove(handled.iterator().next());
        return true;
    }
    public synchronized boolean contains(String id) { return handled.contains(id); }
    public synchronized String[] snapshot() { return handled.toArray(new String[0]); }
    public synchronized void restore(String[] saved) {
        if (saved != null) for (String id : saved) claim(id);
    }
}
