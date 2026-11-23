/* Ticket viewer - read-only. Data: GET /api/tickets (read fresh from .claude/tickets). The project's
   name, id prefix and theme key come from data- attributes that serve.py fills in on <html>. */
(function () {
	"use strict";

	// A status missing here (a project's own schema may add one) still shows, as a neutral pill.
	// user_review (waits on the user) and verified (reviewed and green, only landing it left) are 1.4.0 (B59, B60).
	var status_order = ["open", "in_progress", "in_review", "user_review", "verified", "blocked", "done", "wont_do"];
	// "open" is stored; the user asked to see it as TODO (2026-09-30).
	var status_labels = { open: "TODO", in_progress: "In progress", in_review: "In review", user_review: "User review", verified: "Verified", blocked: "Blocked", done: "Done", wont_do: "Won't do" };
	var severity_order = ["critical", "high", "medium", "low"];
	var severity_labels = { critical: "Critical", high: "High", medium: "Medium", low: "Low" };
	// Every status but done and wont_do (tickets.py's active_statuses).
	var active_statuses = ["open", "in_progress", "in_review", "user_review", "verified", "blocked"];
	var page_data = document.documentElement.dataset;
	// config.json's page_title, used as written (serve.py fills it in); without it, or empty, "<store_name> Tickets".
	var page_title = page_data.pageTitle || (page_data.storeName || "Project") + " Tickets";
	var theme_key = page_data.themeKey || "tickets-theme";
	// config.json's id_prefix, taken literally; any capitals-and-hyphen prefix when the page lacks it.
	var id_prefix_source = /^[A-Z][A-Z0-9]*-$/.test(page_data.idPrefix || "") ? escape_regex(page_data.idPrefix) : "[A-Z][A-Z0-9]*-";
	// config.json's longest ticket number (tickets.py's id_max_digits, which serve.py fills in); 9 when the page lacks it.
	var id_max_digits = /^[1-9][0-9]?$/.test(page_data.idMaxDigits || "") ? page_data.idMaxDigits : "9";
	// A mention of a ticket in text, as tickets.py's mention_id_pattern: no word character or hyphen before the id,
	// 4 to id_max_digits digits, and no word character after (Python's \w is Unicode: letters, digits, _).
	var ticket_id_pattern = mention_pattern();
	// Code that mentions are not taken from: fenced blocks and inline code spans (as tickets.py's code_pattern).
	var code_pattern = /```[\s\S]*?(?:```|$(?![\s\S]))|~~~[\s\S]*?(?:~~~|$(?![\s\S]))|`[^`\n]*`/g;
	// The detail view shows this many comments, then this many more per click on "load more".
	var comment_page_size = 10;
	var hash_id_pattern = new RegExp("^#(" + id_prefix_source + "\\d{4,})$");
	// Matches the phone breakpoint in style.css: below it, list and detail are separate views.
	var single_view_query = window.matchMedia("(max-width: 899px)");
	var theme_colors = { dark: "#2d2d30", light: "#eeeef2" };
	// A tag's colour is used only when written #rrggbb (as tags.json requires); anything else, and a tag the store
	// does not hold, shows in this neutral grey.
	var tag_color_pattern = /^#[0-9a-fA-F]{6}$/;
	var neutral_tag_color = "#808080";
	// A ticket's tag is an id (a positive integer, or its digits as a string) or, until the store is converted, a name.
	var tag_digits_pattern = /^[0-9]+$/;
	// The tag store format whose ids are real. An older store (tag_format 1) is numbered by the server in name order for
	// listing only, so a ticket's integer or digit-string tag finds nothing there and shows as unknown, as tickets.py shows it.
	var numbered_tag_format = 2;

	var columns = [
		{ key: "id", label: "Key" },
		{ key: "summary", label: "Summary" },
		{ key: "severity", label: "Severity" },
		{ key: "status", label: "Status" },
		{ key: "owner_area", label: "Owner" },
		{ key: "updated", label: "Updated" }
	];

	var state = {
		tickets: [],
		tags: index_tags({}),
		selected: null,
		sort_key: "severity",
		sort_descending: false,
		opened_from_list: false,
		// How many comments the detail view shows, for which ticket: a reload keeps it, another ticket starts again.
		comment_limit: 10,
		comment_ticket: null,
		filters: { status: "active", severity: "", tag: "", owner: "", target: "", text: "" }
	};

	var icons = {
		refresh: "M13.5 2v4h-4l1.6-1.6A4.5 4.5 0 1 0 12.4 10l1.3.6A6 6 0 1 1 12.2 3.3L13.5 2z",
		moon: "M9.5 1.5a6.5 6.5 0 1 0 5 10.6A5.5 5.5 0 0 1 9.5 1.5z",
		sun: "M8 4.5a3.5 3.5 0 1 0 0 7 3.5 3.5 0 0 0 0-7zM7.4 0h1.2v2.5H7.4zM7.4 13.5h1.2V16H7.4zM0 7.4h2.5v1.2H0zM13.5 7.4H16v1.2h-2.5zM2.1 3l.9-.9 1.8 1.8-.9.9zM11.2 12.1l.9-.9 1.8 1.8-.9.9zM2.1 13l1.8-1.8.9.9L3 13.9zM11.2 3.9 13 2.1l.9.9-1.8 1.8z",
		back: "M10.7 2.3 5 8l5.7 5.7-1 1L3 8l6.7-6.7z",
		critical: "M8 1 2 7l1.1 1.1L8 3.2l4.9 4.9L14 7zM8 7 2 13l1.1 1.1L8 9.2l4.9 4.9L14 13z",
		high: "M8 4 2 10l1.1 1.1L8 6.2l4.9 4.9L14 10z",
		medium: "M2 5h12v1.6H2zM2 9.4h12V11H2z",
		low: "M8 12 2 6l1.1-1.1L8 9.8l4.9-4.9L14 6z",
		sort_up: "M8 4 3 10h10z",
		sort_down: "M8 12 3 6h10z",
		more: "M8 12 2 6l1.1-1.1L8 9.8l4.9-4.9L14 6z"
	};

	function escape_regex(text) {
		return text.replace(/[.*+?^${}()|[\]\\]/g, "\\$&");
	}

	// ticket_id_pattern: no capture group, since text.replace's callback takes (match, offset). A browser without
	// lookbehind (Safari before 16.4) gets the older word-boundary form rather than no page.
	function mention_pattern() {
		var id = id_prefix_source + "[0-9]{4," + id_max_digits + "}";
		try {
			return new RegExp("(?<![\\p{L}\\p{N}_-])" + id + "(?![\\p{L}\\p{N}_])", "gu");
		} catch (error) {
			return new RegExp("\\b" + id + "(?![0-9A-Za-z_])", "g");
		}
	}

	function $(id) {
		return document.getElementById(id);
	}

	function svg_icon(name) {
		var ns = "http://www.w3.org/2000/svg";
		var svg = document.createElementNS(ns, "svg");
		svg.setAttribute("viewBox", "0 0 16 16");
		svg.setAttribute("aria-hidden", "true");
		var path = document.createElementNS(ns, "path");
		path.setAttribute("d", icons[name]);
		path.setAttribute("fill", "currentColor");
		svg.appendChild(path);
		return svg;
	}

	function el(tag, class_name, text) {
		var node = document.createElement(tag);
		if (class_name) {
			node.className = class_name;
		}
		if (text !== undefined && text !== null) {
			node.textContent = text;
		}
		return node;
	}

	function append(parent, children) {
		children.forEach(function (child) {
			if (child) {
				parent.appendChild(child);
			}
		});
		return parent;
	}

	/* --- theme ---------------------------------------------------------------------------- */

	function stored_theme() {
		try {
			return window.localStorage.getItem(theme_key);
		} catch (error) {
			return null;
		}
	}

	function apply_theme(theme) {
		document.documentElement.setAttribute("data-theme", theme);
		var meta = document.querySelector('meta[name="theme-color"]');
		if (meta) {
			meta.setAttribute("content", theme_colors[theme]);
		}
		var button = $("theme-button");
		button.textContent = "";
		button.appendChild(svg_icon(theme === "dark" ? "sun" : "moon"));
		button.title = theme === "dark" ? "Switch to Light theme" : "Switch to Dark theme";
	}

	function toggle_theme() {
		var next = document.documentElement.getAttribute("data-theme") === "dark" ? "light" : "dark";
		apply_theme(next);
		try {
			window.localStorage.setItem(theme_key, next);
		} catch (error) {
			/* private mode or blocked storage: the choice lasts for this visit only */
		}
	}

	/* --- data ----------------------------------------------------------------------------- */

	function load() {
		return fetch("/api/tickets", { cache: "no-store" })
			.then(function (response) {
				if (!response.ok) {
					throw new Error("HTTP " + response.status);
				}
				return response.json();
			})
			.then(function (data) {
				state.tickets = (data.tickets || []).filter(function (ticket) {
					return ticket && typeof ticket === "object" && ticket.id;
				});
				state.tags = index_tags(data.tags, data.tag_format === numbered_tag_format);
				show_problems(data.problems || []);
				fill_filter_options();
				render_all();
				health.refreshed = new Date();
				health.load_error = null;
				render_statusbar();
			})
			.catch(function (error) {
				show_problems(["Could not load /api/tickets: " + error.message]);
				health.load_error = error.message || "no answer";
				render_statusbar();
			});
	}

	/* --- the status bar (B68) ------------------------------------------------------------- */

	/* Left: when the board last refreshed (a load, or a live-stream event) and the newest `updated` across the tickets;
	   an error state while /api/tickets fails or the live stream is down. Right: the installed version (from the page,
	   so a LAN page shows it too), then "current" or the update button (check_update; a page on this PC only). */
	var health = { refreshed: null, load_error: null, live_down: false };
	var installed_version = /^[0-9]+\.[0-9]+\.[0-9]+$/.test(page_data.version || "") ? page_data.version : "";

	function two(value) {
		return (value < 10 ? "0" : "") + value;
	}

	function clock_time(date) {
		return two(date.getHours()) + ":" + two(date.getMinutes()) + ":" + two(date.getSeconds());
	}

	// An ISO 8601 UTC stamp in this browser's local time, "YYYY-MM-DD HH:MM"; the text as given when it does not parse.
	function local_stamp(timestamp) {
		var date = new Date(timestamp);
		if (isNaN(date.getTime())) {
			return String(timestamp);
		}
		return date.getFullYear() + "-" + two(date.getMonth() + 1) + "-" + two(date.getDate()) + " " +
			two(date.getHours()) + ":" + two(date.getMinutes());
	}

	function newest_update() {
		var newest = "";
		var newest_time = -Infinity;
		state.tickets.forEach(function (ticket) {
			var time = Date.parse(ticket.updated);
			if (typeof ticket.updated === "string" && !isNaN(time) && time > newest_time) {
				newest_time = time;
				newest = ticket.updated;
			}
		});
		return newest;
	}

	function render_statusbar() {
		var bar = $("statusbar");
		var refreshed = $("status-refreshed");
		var since = health.refreshed ? " (last refresh " + clock_time(health.refreshed) + ")" : "";
		if (health.load_error) {
			refreshed.textContent = "Cannot reach the viewer: " + health.load_error + since;
		} else if (health.live_down) {
			refreshed.textContent = "Live updates down, the viewer may have stopped" + since;
		} else if (health.refreshed) {
			refreshed.textContent = "Refreshed " + clock_time(health.refreshed);
		} else {
			refreshed.textContent = "Loading...";
		}
		refreshed.title = health.refreshed ? "Last successful load or live event: " + health.refreshed.toString() : "";
		bar.classList.toggle("error", !!(health.load_error || health.live_down));
		var newest = newest_update();
		var latest = $("status-latest");
		latest.textContent = newest ? "Last ticket update " + local_stamp(newest) : state.tickets.length ? "" : "No tickets";
		latest.title = newest ? newest : "";
		var version = $("status-version");
		version.textContent = installed_version ? "v" + installed_version : "version unknown";
		version.title = installed_version ? "Ticket system " + installed_version + " (this copy)" : "No VERSION file in the tool folder";
	}

	/* Live updates: the server pushes the ticket folder's version on connect and whenever a file
	   changes; a new version reloads. A reconnect after sleep compares too, so nothing is missed. */
	var live_version = null;

	function connect_live() {
		if (!window.EventSource) {
			return;
		}
		var source = new EventSource("/api/events");
		source.addEventListener("tickets", function (event) {
			if (live_version !== null && event.data !== live_version) {
				load();
			} else {
				// The stream answered: the board is as fresh as the folder (a changed version refreshes through load).
				health.refreshed = new Date();
			}
			live_version = event.data;
			health.live_down = false;
			render_statusbar();
			$("refresh-button").classList.add("live");
		});
		source.addEventListener("error", function () {
			$("refresh-button").classList.remove("live");
			// Fired on every failed (re)connect; the browser keeps retrying, and the next event clears it.
			health.live_down = source.readyState !== 1;
			render_statusbar();
		});
	}

	function show_problems(problems) {
		var box = $("problems");
		box.textContent = "";
		if (!problems.length) {
			box.hidden = true;
			return;
		}
		box.appendChild(el("strong", null, "Problems in the ticket store (run tickets.py validate):"));
		var list = el("ul");
		problems.forEach(function (problem) {
			list.appendChild(el("li", null, problem));
		});
		box.appendChild(list);
		box.hidden = false;
	}

	function find_ticket(id) {
		for (var index = 0; index < state.tickets.length; index++) {
			if (state.tickets[index].id === id) {
				return state.tickets[index];
			}
		}
		return null;
	}

	/* --- tags: the store maps an id to its name, colour and description ------------------- */

	/* The store's map (id string -> {name, color, description}) as lookups by id and by name, plus the ids in name
	   order. Every value comes from a file: an entry without a usable id or name is skipped, a colour that is not
	   #rrggbb becomes the neutral one, and the lookups have no prototype, so a name such as "constructor" finds
	   nothing it should not. numbered is true only for a store whose ids are real (numbered_tag_format): resolve_tag looks
	   a ticket's id up only then. */
	function index_tags(map, numbered) {
		var index = { by_id: Object.create(null), by_name: Object.create(null), ids: [], numbered: numbered === true };
		if (!map || typeof map !== "object" || Array.isArray(map)) {
			return index;
		}
		Object.keys(map).forEach(function (key) {
			var entry = map[key];
			if (!/^[1-9][0-9]*$/.test(key) || !entry || typeof entry !== "object" || typeof entry.name !== "string" || !entry.name) {
				return;
			}
			var tag = {
				id: key,
				name: entry.name,
				color: typeof entry.color === "string" && tag_color_pattern.test(entry.color) ? entry.color : neutral_tag_color,
				description: typeof entry.description === "string" ? entry.description : "",
				known: true
			};
			index.by_id[key] = tag;
			if (!(entry.name in index.by_name)) {
				index.by_name[entry.name] = tag;
			}
			index.ids.push(key);
		});
		index.ids.sort(function (a, b) {
			return index.by_id[a].name.localeCompare(index.by_id[b].name) || Number(a) - Number(b);
		});
		return index;
	}

	/* One value of a ticket's tags as the tag it names: an integer or a digit string by id, any other string by name.
	   A value the store does not hold comes back as an unknown tag carrying the raw value, shown in neutral grey. So
	   does every id while the store is not numbered (index_tags): its ids are provisional and name no tag yet. */
	function resolve_tag(value) {
		var tags = state.tags;
		var key = null;
		if (typeof value === "number" && Math.floor(value) === value && value > 0) {
			key = String(value);
		} else if (typeof value === "string" && tag_digits_pattern.test(value)) {
			key = String(parseInt(value, 10));
		}
		var found = key !== null ? (tags.numbered ? tags.by_id[key] : undefined) :
			typeof value === "string" ? tags.by_name[value] : undefined;
		if (found) {
			return found;
		}
		return { id: null, name: typeof value === "string" ? value : JSON.stringify(value), color: neutral_tag_color, description: "", known: false };
	}

	function ticket_tags(ticket) {
		return Array.isArray(ticket.tags) ? ticket.tags : [];
	}

	function has_tag(ticket, id) {
		return ticket_tags(ticket).some(function (value) {
			return resolve_tag(value).id === id;
		});
	}

	function tag_names(ticket) {
		return ticket_tags(ticket).map(function (value) {
			return resolve_tag(value).name;
		});
	}

	/* --- filters -------------------------------------------------------------------------- */

	function set_options(select, options, value) {
		select.textContent = "";
		options.forEach(function (option) {
			var node = el("option", null, option[1]);
			node.value = option[0];
			select.appendChild(node);
		});
		select.value = value;
		if (select.value !== value) {
			select.value = options.length ? options[0][0] : "";
		}
	}

	function distinct(field) {
		var seen = {};
		state.tickets.forEach(function (ticket) {
			if (ticket[field]) {
				seen[ticket[field]] = true;
			}
		});
		return Object.keys(seen).sort();
	}

	function fill_filter_options() {
		var filters = state.filters;
		set_options($("filter-status"), [["", "All statuses"], ["active", "Active"]]
			.concat(status_order.map(function (status) { return [status, status_labels[status]]; })), filters.status);
		set_options($("filter-severity"), [["", "All severities"]]
			.concat(severity_order.map(function (severity) { return [severity, severity_labels[severity]]; })), filters.severity);
		set_options($("filter-tag"), [["", "All tags"]]
			.concat(state.tags.ids.map(function (id) { return [id, state.tags.by_id[id].name]; })), filters.tag);
		set_options($("filter-owner"), [["", "All owners"]]
			.concat(distinct("owner_area").map(function (owner) { return [owner, owner]; })), filters.owner);
		set_options($("filter-target"), [["", "All patches"]]
			.concat(distinct("target_patch").map(function (target) { return [target, target]; })), filters.target);
		var sort_options = [];
		columns.forEach(function (column) {
			sort_options.push([column.key + ":asc", column.label + (column.key === "updated" ? " (oldest first)" : " (ascending)")]);
			sort_options.push([column.key + ":desc", column.label + (column.key === "updated" ? " (newest first)" : " (descending)")]);
		});
		set_options($("sort-select"), sort_options, state.sort_key + (state.sort_descending ? ":desc" : ":asc"));
	}

	function read_filters() {
		state.filters.status = $("filter-status").value;
		state.filters.severity = $("filter-severity").value;
		state.filters.tag = $("filter-tag").value;
		state.filters.owner = $("filter-owner").value;
		state.filters.target = $("filter-target").value;
	}

	function passes(ticket, ignore_status) {
		var filters = state.filters;
		if (!ignore_status && filters.status) {
			if (filters.status === "active" ? active_statuses.indexOf(ticket.status) < 0 : ticket.status !== filters.status) {
				return false;
			}
		}
		if (filters.severity && ticket.severity !== filters.severity) {
			return false;
		}
		// The filter holds a tag id; a ticket matches whether it carries that tag as the id or, unconverted, as the name.
		if (filters.tag && !has_tag(ticket, filters.tag)) {
			return false;
		}
		if (filters.owner && ticket.owner_area !== filters.owner) {
			return false;
		}
		if (filters.target && ticket.target_patch !== filters.target) {
			return false;
		}
		if (filters.text) {
			var haystack = (ticket.id + "\n" + ticket.summary + "\n" + (ticket.body || "") + "\n" + tag_names(ticket).join(" ")).toLowerCase();
			if (haystack.indexOf(filters.text.toLowerCase()) < 0) {
				return false;
			}
		}
		return true;
	}

	function active_filter_count() {
		var filters = state.filters;
		return ["severity", "tag", "owner", "target"].filter(function (key) { return filters[key]; }).length
			+ (filters.status && filters.status !== "active" ? 1 : 0);
	}

	/* --- sorting -------------------------------------------------------------------------- */

	function rank(list, value) {
		var index = list.indexOf(value);
		return index < 0 ? list.length : index;
	}

	function id_number(id) {
		return parseInt(String(id).replace(/\D/g, ""), 10) || 0;
	}

	function compare(a, b) {
		var key = state.sort_key;
		var result = 0;
		if (key === "severity") {
			result = rank(severity_order, a.severity) - rank(severity_order, b.severity);
		} else if (key === "status") {
			result = rank(status_order, a.status) - rank(status_order, b.status);
		} else if (key === "id") {
			result = id_number(a.id) - id_number(b.id);
		} else {
			result = String(a[key] || "").localeCompare(String(b[key] || ""));
		}
		if (state.sort_descending) {
			result = -result;
		}
		return result || id_number(a.id) - id_number(b.id);
	}

	function set_sort(key) {
		if (state.sort_key === key) {
			state.sort_descending = !state.sort_descending;
		} else {
			state.sort_key = key;
			state.sort_descending = key === "updated";
		}
		$("sort-select").value = state.sort_key + (state.sort_descending ? ":desc" : ":asc");
		render_list();
	}

	/* --- pieces --------------------------------------------------------------------------- */

	/* A chip for one value of a ticket's tags. Text and title only: names and descriptions come from files. */
	function tag_chip(value) {
		var tag = resolve_tag(value);
		var chip = el("span", tag.known ? "tag" : "tag tag-unknown", tag.name);
		chip.style.setProperty("--tag-color", tag.color);
		if (!tag.known) {
			chip.title = "Not in tags.json";
		} else if (tag.description) {
			chip.title = tag.description;
		}
		return chip;
	}

	function severity_badge(severity) {
		var badge = el("span", "severity sev-" + severity);
		badge.appendChild(svg_icon(severity_order.indexOf(severity) >= 0 ? severity : "medium"));
		badge.appendChild(el("span", "severity-label", severity_labels[severity] || severity));
		badge.title = "Severity: " + (severity_labels[severity] || severity);
		return badge;
	}

	function status_pill(status) {
		return el("span", "pill st-" + status, status_labels[status] || status);
	}

	function short_date(timestamp) {
		return String(timestamp || "").slice(0, 10);
	}

	/* --- list ----------------------------------------------------------------------------- */

	function render_status_strip() {
		var strip = $("status-strip");
		strip.textContent = "";
		var counts = {};
		var total = 0;
		state.tickets.forEach(function (ticket) {
			if (passes(ticket, true)) {
				counts[ticket.status] = (counts[ticket.status] || 0) + 1;
				total += 1;
			}
		});
		var active = active_statuses.reduce(function (sum, status) { return sum + (counts[status] || 0); }, 0);
		var entries = [["active", "Active", active], ["", "All", total]].concat(status_order.map(function (status) {
			return [status, status_labels[status], counts[status] || 0];
		}));
		entries.forEach(function (entry) {
			// The user scans for what waits on them: user_review's count stands out while it is above 0.
			var button = el("button", "count-button" + (entry[0] === "user_review" && entry[2] > 0 ? " count-attention" : ""));
			button.type = "button";
			button.setAttribute("aria-pressed", state.filters.status === entry[0] ? "true" : "false");
			append(button, [el("span", null, entry[1]), el("b", null, String(entry[2]))]);
			button.addEventListener("click", function () {
				state.filters.status = state.filters.status === entry[0] ? "active" : entry[0];
				$("filter-status").value = state.filters.status;
				render_list();
			});
			strip.appendChild(button);
		});
	}

	function render_head() {
		var head = el("div", "list-head");
		head.setAttribute("role", "row");
		columns.forEach(function (column) {
			var button = el("button", "sort-button" + (state.sort_key === column.key ? " active" : ""), column.label);
			button.type = "button";
			button.style.gridArea = column.key === "id" ? "key" : column.key === "owner_area" ? "owner" : column.key;
			if (state.sort_key === column.key) {
				button.appendChild(svg_icon(state.sort_descending ? "sort_down" : "sort_up"));
				button.setAttribute("aria-sort", state.sort_descending ? "descending" : "ascending");
			}
			button.addEventListener("click", function () {
				set_sort(column.key);
			});
			head.appendChild(button);
		});
		return head;
	}

	function render_row(ticket) {
		var row = el("a", "row" + (ticket.id === state.selected ? " selected" : ""));
		row.href = "#" + ticket.id;
		row.setAttribute("role", "listitem");
		row.dataset.id = ticket.id;
		var summary = el("span", "c-summary");
		summary.appendChild(el("span", "summary-text", ticket.summary));
		if (ticket_tags(ticket).length) {
			append(summary, [append(el("span", "tags"), ticket_tags(ticket).map(tag_chip))]);
		}
		var severity = append(el("span", "c-severity"), [severity_badge(ticket.severity)]);
		var status = append(el("span", "c-status"), [status_pill(ticket.status)]);
		var updated = el("span", "c-updated muted", short_date(ticket.updated));
		updated.title = "Updated " + ticket.updated;
		append(row, [el("span", "c-key", ticket.id), summary, severity, status,
			el("span", "c-owner muted", ticket.owner_area), updated]);
		row.addEventListener("click", function () {
			state.opened_from_list = true;
		});
		return row;
	}

	function render_list() {
		render_status_strip();
		var list = $("list");
		list.textContent = "";
		var shown = state.tickets.filter(function (ticket) { return passes(ticket, false); }).sort(compare);
		list.appendChild(render_head());
		if (!shown.length) {
			list.appendChild(el("div", "empty", state.tickets.length ? "No tickets match these filters." : "No tickets yet."));
		}
		shown.forEach(function (ticket) {
			list.appendChild(render_row(ticket));
		});
		var toggle = $("filter-toggle");
		var count = active_filter_count();
		toggle.textContent = count ? "Filters (" + count + ")" : "Filters";
		toggle.classList.toggle("active", count > 0);
	}

	/* --- detail --------------------------------------------------------------------------- */

	function render_markdown(text) {
		var container = el("div", "markdown");
		var html = window.marked.parse(text || "", { gfm: true, breaks: false });
		container.innerHTML = window.DOMPurify.sanitize(html, { USE_PROFILES: { html: true } });
		container.querySelectorAll("table").forEach(function (table) {
			var wrap = el("div", "table-wrap");
			table.parentNode.insertBefore(wrap, table);
			wrap.appendChild(table);
		});
		container.querySelectorAll('li > input[type="checkbox"]').forEach(function (box) {
			box.disabled = true;
			box.parentNode.classList.add("task");
		});
		container.querySelectorAll("a[href]").forEach(function (link) {
			var href = link.getAttribute("href");
			if (href.charAt(0) !== "#") {
				link.target = "_blank";
				link.rel = "noopener noreferrer";
			}
		});
		link_ticket_ids(container);
		return container;
	}

	/* Turn bare mentions of this store's ids (prefix and digits) into links, outside code and existing links. */
	function link_ticket_ids(root) {
		var walker = document.createTreeWalker(root, NodeFilter.SHOW_TEXT, {
			acceptNode: function (node) {
				if (!node.nodeValue || node.nodeValue.search(ticket_id_pattern) < 0) {
					return NodeFilter.FILTER_REJECT;
				}
				for (var parent = node.parentNode; parent && parent !== root; parent = parent.parentNode) {
					if (/^(A|CODE|PRE)$/.test(parent.nodeName)) {
						return NodeFilter.FILTER_REJECT;
					}
				}
				return NodeFilter.FILTER_ACCEPT;
			}
		});
		var targets = [];
		while (walker.nextNode()) {
			targets.push(walker.currentNode);
		}
		targets.forEach(function (node) {
			var text = node.nodeValue;
			var fragment = document.createDocumentFragment();
			var last = 0;
			text.replace(ticket_id_pattern, function (match, offset) {
				fragment.appendChild(document.createTextNode(text.slice(last, offset)));
				// Text and attributes only: the id matched the pattern, the summary comes from a file.
				var other = find_ticket(match);
				var link;
				if (other) {
					link = el("a", "ticket-ref", match);
					link.href = "#" + match;
					link.title = String(other.summary || "");
				} else {
					link = el("span", "ticket-ref ticket-missing", match);
					link.title = "Not found in this store";
				}
				fragment.appendChild(link);
				last = offset + match.length;
				return match;
			});
			fragment.appendChild(document.createTextNode(text.slice(last)));
			node.parentNode.replaceChild(fragment, node);
		});
	}

	function meta_item(label, value_node) {
		return append(el("div", "meta-item"), [el("span", "meta-label", label), append(el("div", "meta-value"), [value_node])]);
	}

	/* A list from a ticket file: its strings only, anything else (a hand-edited file) dropped. */
	function string_list(value) {
		return Array.isArray(value) ? value.filter(function (item) { return typeof item === "string"; }) : [];
	}

	/* The ticket ids a ticket's body and comments mention, outside code, once each, without its own (as tickets.py's
	   mentioned_ids). Derived from the text, never stored. */
	function mentioned_ids(ticket) {
		var texts = [ticket.body].concat(ticket_comments(ticket).map(function (comment) { return comment.body; }));
		var found = [];
		texts.forEach(function (text) {
			if (typeof text !== "string") {
				return;
			}
			(text.replace(code_pattern, " ").match(ticket_id_pattern) || []).forEach(function (id) {
				if (id !== ticket.id && found.indexOf(id) < 0) {
					found.push(id);
				}
			});
		});
		return found;
	}

	function mentioned_by(ticket) {
		return state.tickets.filter(function (other) {
			return other.id !== ticket.id && mentioned_ids(other).indexOf(ticket.id) >= 0;
		}).map(function (other) { return other.id; });
	}

	/* One linked ticket: its id, a link that opens it, and its summary when the store has it. */
	function ticket_link_item(id) {
		var item = el("li");
		var other = find_ticket(id);
		if (other) {
			var link = el("a", "ticket-link", null);
			link.href = "#" + id;
			append(link, [el("span", "mono", id), el("span", "ticket-link-summary", " - " + String(other.summary || ""))]);
			item.appendChild(link);
		} else {
			item.appendChild(el("span", "mono ticket-missing", id));
			item.appendChild(el("span", "muted", " - not found in this store"));
		}
		return item;
	}

	/* The links section: the typed ticket links, the untyped links.tickets (read as relates-to), the mentions both ways,
	   then files and commits. Each ticket entry opens that ticket. */
	function links_block(ticket) {
		var links = ticket.links && typeof ticket.links === "object" ? ticket.links : {};
		var groups = [];
		[
			["Blocked by", string_list(links.blocked_by)],
			["Blocks", string_list(links.blocks)],
			["Relates to", string_list(links.relates_to)],
			["Linked", string_list(links.tickets)],
			["Mentions", mentioned_ids(ticket)],
			["Mentioned by", mentioned_by(ticket)]
		].forEach(function (group) {
			if (group[1].length) {
				groups.push(append(el("div", "links-group"), [el("span", "meta-label", group[0]),
					append(el("ul", "ticket-links"), group[1].map(ticket_link_item))]));
			}
		});
		[["files", "Files"], ["commits", "Commits"]].forEach(function (kind) {
			var values = string_list(links[kind[0]]);
			if (values.length) {
				var list = el("ul");
				values.forEach(function (value) {
					list.appendChild(append(el("li"), [el("code", null, value)]));
				});
				groups.push(append(el("div", "links-group"), [el("span", "meta-label", kind[1]), list]));
			}
		});
		return groups.length ? append(el("section", "links"), [el("h2", "section-title", "Links")].concat(groups)) : null;
	}

	/* The comments a ticket file holds that read as comments, newest first (by time; the file's order breaks ties). */
	function ticket_comments(ticket) {
		var comments = Array.isArray(ticket.comments) ? ticket.comments : [];
		return comments.map(function (comment, index) { return { comment: comment, index: index }; })
			.filter(function (entry) {
				return entry.comment && typeof entry.comment === "object" && typeof entry.comment.body === "string";
			})
			.sort(function (a, b) {
				var at_a = String(a.comment.at || "");
				var at_b = String(b.comment.at || "");
				return at_a < at_b ? 1 : at_a > at_b ? -1 : b.index - a.index;
			})
			.map(function (entry) { return entry.comment; });
	}

	function comment_node(comment) {
		var when = el("span", "comment-at muted", String(comment.at || "").replace("T", " ").replace("Z", " UTC"));
		when.title = String(comment.at || "");
		var head = append(el("div", "comment-head"), [el("span", "comment-author", String(comment.author || "?")), when]);
		return append(el("li", "comment"), [head, render_markdown(comment.body)]);
	}

	/* The comments section, newest first: the first comment_limit shown, the rest not rendered until "load more" asks
	   for the next comment_page_size. */
	function comments_block(ticket) {
		var comments = ticket_comments(ticket);
		if (!comments.length) {
			return null;
		}
		var list = el("ol", "comments");
		var more = el("button", "load-more");
		more.type = "button";
		var shown = 0;
		function show_up_to(limit) {
			for (; shown < Math.min(limit, comments.length); shown++) {
				list.appendChild(comment_node(comments[shown]));
			}
			var hidden = comments.length - shown;
			more.hidden = hidden <= 0;
			more.textContent = "";
			append(more, [svg_icon("more"), el("span", null, "Load " + Math.min(comment_page_size, hidden) + " more (" + hidden + " hidden)")]);
		}
		more.addEventListener("click", function () {
			state.comment_limit = shown + comment_page_size;
			show_up_to(state.comment_limit);
		});
		show_up_to(state.comment_limit);
		return append(el("section", "comments-section"), [el("h2", "section-title", "Comments (" + comments.length + ")"),
			list, more]);
	}

	function detail_bar(ticket_id) {
		var back = el("button", "back-button");
		back.type = "button";
		back.setAttribute("aria-label", "Back to the ticket list");
		append(back, [svg_icon("back"), el("span", null, "Tickets")]);
		back.addEventListener("click", go_back);
		return append(el("div", "detail-bar"), [back, el("span", "detail-key", ticket_id || "")]);
	}

	function render_detail() {
		var pane = $("detail");
		pane.textContent = "";
		var ticket = state.selected ? find_ticket(state.selected) : null;
		if (!ticket) {
			pane.appendChild(detail_bar(state.selected));
			pane.appendChild(el("div", "placeholder", state.selected
				? "No ticket " + state.selected + " in .claude/tickets."
				: "Select a ticket to see it here."));
			return;
		}
		pane.appendChild(detail_bar(ticket.id));
		if (state.comment_ticket !== ticket.id) {
			state.comment_ticket = ticket.id;
			state.comment_limit = comment_page_size;
		}
		var detail = el("article", "detail");
		var tags = ticket_tags(ticket).length ? append(el("div", "tags detail-tags"), ticket_tags(ticket).map(tag_chip)) : null;
		var created = el("span", null, short_date(ticket.created));
		created.title = ticket.created;
		var updated = el("span", null, short_date(ticket.updated));
		updated.title = ticket.updated;
		var meta = append(el("div", "meta"), [
			meta_item("Status", status_pill(ticket.status)),
			meta_item("Severity", severity_badge(ticket.severity)),
			meta_item("Owner", el("span", null, ticket.owner_area)),
			meta_item("Target patch", el("span", null, ticket.target_patch)),
			meta_item("Author", el("span", null, ticket.author)),
			meta_item("Created", created),
			meta_item("Updated", updated)
		]);
		// The order the user asked for (B58): the subject, its tags, the body, the links, then the comments. The fields
		// (status, severity, ...) sit between the tags and the body.
		var body = append(el("section", "body-section"), [el("h2", "section-title", "Description"), render_markdown(ticket.body)]);
		append(detail, [el("div", "detail-key", ticket.id), el("h1", null, ticket.summary), tags, meta, body,
			links_block(ticket), comments_block(ticket)]);
		pane.appendChild(detail);
		pane.scrollTop = 0;
		document.title = ticket.id + " - " + page_title;
	}

	/* --- routing: the hash selects a ticket, so a link can point at one ------------------- */

	function route() {
		var match = hash_id_pattern.exec(window.location.hash);
		state.selected = match ? match[1] : null;
		document.body.classList.toggle("show-detail", !!state.selected);
		if (!state.selected) {
			document.title = page_title;
			state.opened_from_list = false;
		}
		render_list();
		render_detail();
		var row = document.querySelector('.row[data-id="' + state.selected + '"]');
		if (row && !single_view_query.matches) {
			row.scrollIntoView({ block: "nearest" });
		}
	}

	function go_back() {
		if (state.opened_from_list) {
			window.history.back();
		} else {
			window.location.hash = "";
		}
	}

	function render_all() {
		route();
	}

	/* --- filter sheet (phone) ------------------------------------------------------------- */

	function set_sheet(open) {
		document.body.classList.toggle("filters-open", open);
		$("sheet-backdrop").hidden = !open;
		$("filter-toggle").setAttribute("aria-expanded", open ? "true" : "false");
	}

	/* --- the one-click update (B65) ------------------------------------------------------- */

	/* GET /api/update says whether a newer release is on offer; the badge opens a confirm dialog with its changelog. The
	   apply is a POST carrying the token serve.py put in the page (only a browser on this PC gets one). While the helper
	   restarts the viewer, the page polls /api/health, then reloads and shows the result. */
	var update_info = null;
	var update_poll_ms = 1000;
	// Give up waiting after this long (an export, a validate of a big store and a restart fit well inside it).
	var update_wait_ms = 10 * 60 * 1000;
	var update_started_at = 0;
	var update_viewer_down = false;
	var update_in_progress = false;
	var update_result_key = theme_key + "-update-result";
	var update_final_states = ["done", "rolled_back", "failed"];

	function update_token() {
		var meta = document.querySelector('meta[name="tickets-update-token"]');
		var value = meta ? meta.getAttribute("content") || "" : "";
		// An unfilled template placeholder is no token.
		return /^[A-Za-z0-9_-]{16,}$/.test(value) ? value : "";
	}

	function get_json(url) {
		return fetch(url, { cache: "no-store" }).then(function (response) {
			if (!response.ok) {
				throw new Error("HTTP " + response.status);
			}
			return response.json();
		});
	}

	function check_update() {
		// Only a page on this PC (serve.py gave it the token) asks; a LAN page shows the version alone.
		if (!update_token()) {
			$("update-badge").hidden = true;
			$("status-current").hidden = true;
			return Promise.resolve(null);
		}
		return get_json("/api/update").then(function (info) {
			update_info = info;
			var badge = $("update-badge");
			if (info.can_apply && info.latest && update_token()) {
				badge.textContent = "Update " + info.latest;
				badge.title = "Release " + info.latest + " is available (this copy runs " + info.installed + ")";
				badge.hidden = false;
			} else {
				badge.hidden = true;
			}
			var current = $("status-current");
			current.hidden = info.state !== "current";
			current.title = info.latest ? "The newest tagged release is " + info.latest : "";
			if (info.busy && !update_in_progress) {
				// Opened (or reloaded) while an update runs.
				begin_waiting(info.status && info.status.version, true);
			}
			return info;
		}).catch(function () {
			$("update-badge").hidden = true;
			$("status-current").hidden = true;
		});
	}

	function dialog_button(label, primary, handler) {
		var button = el("button", "update-button" + (primary ? " primary" : ""), label);
		button.type = "button";
		button.addEventListener("click", handler);
		return button;
	}

	function open_dialog(title, nodes, buttons) {
		var dialog = $("update-dialog");
		dialog.textContent = "";
		var heading = el("h2", null, title);
		heading.id = "update-title";
		var actions = append(el("div", "update-actions"), buttons);
		append(dialog, [heading, append(el("div", "update-body"), nodes), buttons.length ? actions : null]);
		$("update-backdrop").hidden = false;
		var focus = actions.querySelector("button.primary") || actions.querySelector("button");
		if (focus) {
			focus.focus();
		}
	}

	function close_dialog() {
		if (!update_in_progress) {
			$("update-backdrop").hidden = true;
		}
	}

	function text_list(lines, as_code) {
		return append(el("ul", "update-list"), lines.map(function (line) {
			return as_code ? append(el("li"), [el("code", null, String(line))]) : el("li", null, String(line));
		}));
	}

	function confirm_update() {
		var info = update_info;
		if (!info || !info.latest) {
			return;
		}
		var nodes = [];
		(info.notes || []).forEach(function (note) {
			nodes.push(el("h3", null, "Release " + note.version));
			if (note.changelog) {
				// The changelog comes from the source's manifests: rendered like a ticket body, through DOMPurify.
				nodes.push(render_markdown(note.changelog));
			}
			// The entry's "Agents:" lines are part of the changelog text above, so they are not listed again.
		});
		nodes.push(el("p", "update-note", "The update replaces the tool files and the schema (your enums and id patterns " +
			"are kept), restarts the viewer and runs validate; if anything fails, the backup is put back. The skill, the " +
			"hooks and the rules doc are only staged, in build/tickets/staged/" + info.latest + "/, for review. Nothing " +
			"is committed: review and commit the changes yourself."));
		open_dialog("Update " + info.installed + " -> " + info.latest, nodes, [
			dialog_button("Cancel", false, close_dialog),
			dialog_button("Update now", true, function () { start_update(info.latest); })
		]);
	}

	function show_progress(version, step) {
		open_dialog("Updating" + (version ? " to " + version : ""), [
			append(el("p"), [el("span", null, "Step: "), el("span", "update-step", step || "starting")]),
			el("p", "muted", "Keep this page open. The viewer stops and starts again on its own; this page reloads when " +
				"it is back.")
		], []);
	}

	function start_update(version) {
		begin_waiting(version, false);
		fetch("/api/update/apply", {
			method: "POST",
			cache: "no-store",
			headers: { "Content-Type": "application/json", "X-Tickets-Token": update_token() },
			body: JSON.stringify({ version: version })
		}).then(function (response) {
			return response.json().catch(function () { return {}; }).then(function (data) {
				if (response.status !== 202) {
					throw new Error(data.error || "HTTP " + response.status);
				}
				poll_update(version);
			});
		}).catch(function (error) {
			update_in_progress = false;
			show_result({ state: "failed", detail: "The viewer did not start the update: " + error.message });
		});
	}

	function begin_waiting(version, poll) {
		update_in_progress = true;
		update_viewer_down = false;
		update_started_at = Date.now();
		show_progress(version, "starting");
		if (poll) {
			poll_update(version);
		}
	}

	function poll_update(version) {
		if (Date.now() - update_started_at > update_wait_ms) {
			update_in_progress = false;
			show_result({ state: "failed", detail: "No answer from the updater in time; see build/tickets/update/" +
				"status.json and helper.log." });
			return;
		}
		get_json("/api/update/status").then(function (data) {
			var status = data.status || {};
			if (!data.busy && update_final_states.indexOf(status.state) >= 0) {
				update_in_progress = false;
				if (update_viewer_down) {
					// The viewer restarted: reload to get its new page, then show the result there.
					try {
						window.sessionStorage.setItem(update_result_key, "1");
					} catch (error) {
						/* no storage: the result shows on this page instead */
						show_result(status);
						return;
					}
					window.location.reload();
					return;
				}
				show_result(status);
				return;
			}
			show_progress(status.version || version, status.step);
			window.setTimeout(function () { poll_update(version); }, update_poll_ms);
		}).catch(function () {
			update_viewer_down = true;
			show_progress(version, "the viewer is restarting");
			poll_health(version);
		});
	}

	// While the viewer is down: ask /api/health until it answers, then follow the status again.
	function poll_health(version) {
		window.setTimeout(function () {
			get_json("/api/health").then(function () {
				poll_update(version);
			}).catch(function () {
				if (Date.now() - update_started_at > update_wait_ms) {
					poll_update(version);
				} else {
					poll_health(version);
				}
			});
		}, update_poll_ms);
	}

	function show_result(status) {
		var nodes = [];
		var title;
		if (status.state === "done") {
			title = "Updated to " + (status.version || "the new release");
			var changed = status.changed || [];
			nodes.push(el("h3", null, "Changed files (" + changed.length + "), not committed"));
			nodes.push(changed.length ? text_list(changed, true) : el("p", "muted", "none"));
			var actions = status.actions || [];
			nodes.push(el("h3", null, "ACTION lines (" + actions.length + ")"));
			nodes.push(actions.length ? text_list(actions, false) : el("p", "muted", "none"));
			nodes.push(el("p", "update-note", "Agents find these lines, and what changed for them, in " +
				".claude/tickets/README.md. Review and commit the changed files."));
		} else {
			title = status.state === "rolled_back" ? "Update rolled back" : "Update not applied";
			if (status.step) {
				nodes.push(append(el("p"), [el("span", null, "Failed at: "), el("span", "update-step", status.step)]));
			}
			nodes.push(el("pre", "update-detail", status.detail || "no detail"));
			if (status.problems && status.problems.length) {
				nodes.push(el("h3", null, "Problems"));
				nodes.push(text_list(status.problems, false));
			}
			nodes.push(el("p", "update-note update-error", status.state === "rolled_back"
				? "The backup was put back and the old viewer started again."
				: "Nothing outside build/tickets/ was changed."));
		}
		open_dialog(title, nodes, [dialog_button("Close", true, close_dialog)]);
		check_update();
	}

	function show_remembered_result() {
		var remembered = null;
		try {
			remembered = window.sessionStorage.getItem(update_result_key);
			window.sessionStorage.removeItem(update_result_key);
		} catch (error) {
			return;
		}
		if (remembered) {
			get_json("/api/update/status").then(function (data) {
				if (data.status) {
					show_result(data.status);
				}
			}).catch(function () { /* the result stays in build/tickets/update/status.json */ });
		}
	}

	/* --- wiring --------------------------------------------------------------------------- */

	function init() {
		// ?theme=light|dark forces a theme for this visit (screenshots, a shared link) without storing it.
		var forced = new URLSearchParams(window.location.search).get("theme");
		apply_theme(forced === "light" || forced === "dark" ? forced : stored_theme() === "light" ? "light" : "dark");
		$("theme-button").addEventListener("click", toggle_theme);
		$("refresh-button").appendChild(svg_icon("refresh"));
		$("refresh-button").addEventListener("click", load);
		["filter-status", "filter-severity", "filter-tag", "filter-owner", "filter-target"].forEach(function (id) {
			$(id).addEventListener("change", function () {
				read_filters();
				render_list();
			});
		});
		$("sort-select").addEventListener("change", function () {
			var parts = $("sort-select").value.split(":");
			state.sort_key = parts[0];
			state.sort_descending = parts[1] === "desc";
			render_list();
		});
		$("search").addEventListener("input", function () {
			state.filters.text = $("search").value.trim();
			render_list();
		});
		$("filter-toggle").addEventListener("click", function () { set_sheet(true); });
		$("filter-done").addEventListener("click", function () { set_sheet(false); });
		$("sheet-backdrop").addEventListener("click", function () { set_sheet(false); });
		$("filter-clear").addEventListener("click", function () {
			state.filters = { status: "active", severity: "", tag: "", owner: "", target: "", text: state.filters.text };
			fill_filter_options();
			render_list();
		});
		document.addEventListener("keydown", function (event) {
			if (event.key === "Escape") {
				if (document.body.classList.contains("filters-open")) {
					set_sheet(false);
				} else if (state.selected && single_view_query.matches) {
					go_back();
				}
			}
		});
		window.addEventListener("hashchange", route);
		// Coming back to the app (phone unlock, tab switch) picks up edited tickets.
		document.addEventListener("visibilitychange", function () {
			if (document.visibilityState === "visible") {
				load();
			}
		});
		$("update-badge").addEventListener("click", confirm_update);
		$("update-backdrop").addEventListener("click", function (event) {
			if (event.target === $("update-backdrop")) {
				close_dialog();
			}
		});
		document.addEventListener("keydown", function (event) {
			if (event.key === "Escape" && !$("update-backdrop").hidden) {
				close_dialog();
			}
		});
		render_statusbar();
		load();
		connect_live();
		show_remembered_result();
		check_update();
	}

	init();
}());
