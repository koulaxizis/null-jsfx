// Supporters page: renders supporters.json, Patrons first, then Backers
(function() {
    const list = document.getElementById('supporters-list');
    const updated = document.getElementById('supporters-updated');
    const tiers = ['Patron', 'Backer'];

    function empty(text) {
        list.innerHTML = '';
        const p = document.createElement('p');
        p.className = 'supporters-empty';
        p.textContent = text;
        list.appendChild(p);
    }

    fetch('../supporters.json', { cache: 'no-cache' })
        .then(function(r) { return r.json(); })
        .then(function(data) {
            const people = (data.supporters || []).filter(function(s) { return s && s.name; });
            if (data.updated) updated.textContent = 'Last updated: ' + data.updated;
            if (!people.length) {
                empty('The list is brand new. Your name could be the first one here.');
                return;
            }
            list.innerHTML = '';
            tiers.forEach(function(tier) {
                const names = people.filter(function(s) { return (s.tier || 'Backer') === tier; });
                if (!names.length) return;
                const h = document.createElement('h2');
                h.textContent = tier + 's';
                const ul = document.createElement('ul');
                names.forEach(function(s) {
                    const li = document.createElement('li');
                    li.textContent = s.name;
                    ul.appendChild(li);
                });
                list.appendChild(h);
                list.appendChild(ul);
            });
        })
        .catch(function() { empty('The list could not be loaded. Please try again later.'); });
})();
