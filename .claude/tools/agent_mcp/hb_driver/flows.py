"""The menu flows: main menu, settings mute, account, login, characters, enter / leave the world, quit.

Every flow starts from wherever the client is, drives it with widget ids (never remembered pixels),
and ends by PROVING where it landed (screen id, widget values, get_state.character). A flow that
cannot reach its goal raises common.flow_error saying where it stopped.

Overlay messages ("Account created", "Name taken", ...) report `overlay.ok` (a request in flight
reports `overlay.cancel` once its delay passes), listed first in hb_widgets. While one is up the
page's own widgets report DISABLED, which is how resolve_request tells it is there; it is dismissed
by clicking `overlay.ok` by name (Enter would be unsafe - on character select Enter ENTERS THE GAME).
A client build without those widgets is dismissed with a left click on a point no widget covers. The
reason for a refusal is read from the login server's log, which states it.
"""

import time

from . import bridge
from . import common
from . import server_logs

# This client's screen ids (get_state "screen" is the screen class name). The widget ids below and in the
# flows are still Helbreath-SDL's: they need the widgets command (phase 2) and a rewrite (phase 4).
main_menu = "Screen_MainMenu"
login_screen = "Screen_Login"
account_create = "Screen_CreateAccount"
character_select = "Screen_SelectCharacter"
character_create = "Screen_CreateNewCharacter"
settings_screen = "Screen_Settings"  # no such screen here yet: never matches
in_game = "Screen_OnGame"
exit_screen = "Screen_Quit"

default_timeout_s = 20.0
# An overlay still up this long after a request is treated as a MESSAGE and dismissed (a busy one
# ignores the click, so dismissing early costs nothing but a retry).
message_grace_s = 1.0
poll_s = 0.1
# Points probed for a widget-free dismissal click, logical 640x480 corners inset by two pixels.
neutral_candidates = ((2, 2), (637, 2), (2, 477), (637, 477))
# How many times a flow walks back toward the main menu before giving up.
max_navigation_steps = 8
max_stepper_clicks = 32
enter_world_timeout_ms = 45000
# get_state polls while waiting for the spawn: fast, since each answer costs at most one frame.
spawn_poll_s = 0.005
checkbox_on = "on"
checkbox_off = "off"

# The front-end overlay's widgets (overlay.cpp): ok dismisses a message, cancel aborts a pending request.
overlay_ok = "overlay.ok"
overlay_prefix = "overlay."
# The entering popup's back button, shown when entering the world failed.
entering_back = "entering.back"
# Screens a client is logged in on: the main-menu-only Settings proof cannot run from them.
logged_in_screens = (in_game, character_select, character_create)

login_refusal_needles = ("login rejected",)
account_refusal_needles = ("account create rejected",)
character_refusal_needles = ("character create rejected",)
enter_refusal_needles = ("enter-game rejected", "enter-game refused", "enter-game abandoned")
delete_refusal_needles = ("character delete rejected", "character delete refused")


def _contains(widget, x, y):
	return (widget.get("x", 0) <= x <= widget.get("x", 0) + widget.get("w", 0)
	        and widget.get("y", 0) <= y <= widget.get("y", 0) + widget.get("h", 0))


def neutral_point(listing):
	"""A logical point no reported widget covers, for dismissing a modal message."""
	widgets = listing.get("widgets") or []
	for x, y in neutral_candidates:
		if not any(_contains(widget, x, y) for widget in widgets):
			return x, y
	raise common.flow_error("no widget-free point to dismiss the overlay with")


def dismiss_overlay(sess, listing):
	"""Dismiss a front-end message overlay. True when a click was made.

	Clicks `overlay.ok` by name. Other `overlay.*` widgets alone (a pending request's cancel) mean a
	request is still in flight: nothing to dismiss, and cancelling it would be wrong, so no click. A
	client build that reports no overlay widgets at all gets the widget-free corner click instead.
	"""
	ids = sess.widget_map(listing)
	ok = ids.get(overlay_ok)
	if ok is not None:
		if not ok.get("enabled", True):
			return False
		sess.click_widget(overlay_ok)
		return True
	if any(widget_id.startswith(overlay_prefix) for widget_id in ids):
		return False
	x, y = neutral_point(listing)
	sess.click(x, y)
	return True


def _watch(path):
	"""A login-log watch marked now. None = the dev login log (resolved at call time); False = none."""
	if path is False:
		return None
	try:
		return server_logs.log_watch(common.login_log_path if path is None else path)
	except OSError:
		return None


def _refusal(watch, needles):
	if watch is None:
		return None
	record = server_logs.login_outcome(watch, needles)
	return record["line"] if record else None


def resolve_request(sess, origin, gate_widget, succeeded, timeout_s=default_timeout_s):
	"""After a click that raises a modal overlay (busy, then a message or a screen change).

	`succeeded(state, listing)` says the goal is reached. `gate_widget` is a page widget that reports
	disabled while any overlay is up. Returns True on success, False when a message was dismissed
	and the page came back without success (the request was refused). Raises on a timeout or on an
	unexpected screen.
	"""
	started = time.monotonic()
	last_dismiss = started
	dismissals = 0
	while time.monotonic() - started < timeout_s:
		state = sess.state()
		if state.get("transitioning"):
			time.sleep(poll_s)
			continue
		listing = sess.widgets()
		if succeeded(state, listing):
			return True
		if state.get("screen") != origin:
			raise common.flow_error("expected '{}' or success, landed on '{}'".format(origin, state.get("screen")))
		gate = sess.widget(gate_widget, listing)
		gate_live = gate is not None and gate.get("enabled", True)
		now = time.monotonic()
		if gate_live and dismissals > 0:
			return False
		if gate_live and now - started > message_grace_s:
			return False  # no overlay came up at all: the click did not submit anything
		if not gate_live and now - last_dismiss > message_grace_s:
			if dismiss_overlay(sess, listing):
				dismissals += 1
			last_dismiss = now
		time.sleep(poll_s)
	raise common.flow_error("request on '{}' did not resolve within {:g}s".format(origin, timeout_s))


# --- navigation ------------------------------------------------------------------------------------

def _open_world_settings(sess):
	"""Open the in-game Settings window on its System tab (switch character / log out live there)."""
	ids = sess.widget_map()
	if "settings.title" not in ids:
		sess.click_widget("hud.settings")
		sess.wait_widget("settings.title")
		ids = sess.widget_map()
	if "settings.switch_character" not in ids:
		sess.click_widget("settings.cat_system")  # the System category (settings_dialog.h)
		sess.wait_widget("settings.switch_character")


# The widget that leaves each menu screen toward the main menu.
leave_widgets = {
	login_screen: "login.return",
	account_create: "account_create.cancel",
	character_create: "char_create.cancel",
	character_select: "char_select.log_out",
	settings_screen: "settings.back",
}


def go_to_main_menu(sess, timeout_s=default_timeout_s):
	"""Walk back to the main menu from any screen (logging out of the world if needed). A modal
	message in the way is dismissed; an open delete / password panel is cancelled."""
	for _ in range(max_navigation_steps):
		state = sess.state()
		screen = state.get("screen")
		if state.get("transitioning"):
			time.sleep(poll_s)
			continue
		if screen == main_menu:
			return sess.wait_screen(main_menu, timeout_s)
		listing = sess.widgets()
		ids = sess.widget_map(listing)
		if screen == in_game:
			_open_world_settings(sess)
			sess.click_widget("settings.logout")
		elif screen in leave_widgets:
			panel_close = next((key for key in ("char_select.delete_no", "char_select.password_cancel",
			                                    "settings.preview_done") if key in ids), None)
			leave = ids.get(leave_widgets[screen])
			if panel_close:
				sess.click_widget(panel_close)
				continue
			if leave is None or not leave.get("enabled", True):
				dismiss_overlay(sess, listing)  # a modal message is up: dismiss it first
				time.sleep(message_grace_s)
				continue
			sess.click_widget(leave_widgets[screen])
		else:
			time.sleep(poll_s * 5)  # splash / loading / exit: it moves on by itself
			continue
		common.poll_until(lambda: sess.state().get("screen") != screen, timeout_s, poll_s)
	raise common.flow_error("could not reach the main menu (stuck on '{}')".format(sess.screen()))


# --- settings: master mute -------------------------------------------------------------------------

def _sound_category(ids):
	for widget_id in ids:
		if widget_id.startswith("settings.cat_") and "sound" in widget_id:
			return widget_id
	raise common.flow_error("no Sound category among the Settings widgets: {}".format(sorted(ids)))


# The master mute checkbox's id (settings.sound_<master|ambiance|effect|music|ui>_enabled, client batch 3-4).
master_enabled_id = "settings.sound_master_enabled"


def _master_enabled(ids):
	if master_enabled_id in ids:
		return master_enabled_id
	for widget_id in ids:
		if widget_id.startswith("settings.") and "master" in widget_id and widget_id.endswith("_enabled"):
			return widget_id
	raise common.flow_error("no master '*_enabled' tick among the Settings widgets: {}".format(sorted(ids)))


def set_master_mute(sess, muted, client_json=None, leave_session=False, timeout_s=default_timeout_s):
	"""Set Settings > Sound > Master to muted / playing, from the FRONT-END Settings screen.

	The in-game Settings window reports no setting rows (HB-0158), so this runs from the main menu,
	whose Settings screen reports every row by id. Located by id pattern, not pixels: the category
	`settings.cat_*sound*`, then the tick `settings.sound_master_enabled` (older builds: any
	`settings.*master*_enabled`) whose value is `on` when sound PLAYS. It SETS the state (never a blind
	toggle - client.json may already hold either). When `client_json` is given, the persisted
	`audio.master_muted` is checked too. A client in the world or on character select is muted live with
	the bridge's `set_audio` instead (no logout, `via: set_audio`); leave_session=True takes the Settings
	route anyway, logging it out first. Returns {previous_muted, muted, clicked, widget, via}. The Settings
	route ends on the main menu.
	"""
	if sess.screen() in logged_in_screens and not leave_session:
		return set_audio_mute(sess, muted, client_json, timeout_s)
	go_to_main_menu(sess, timeout_s)
	sess.click_widget("main_menu.settings")
	sess.wait_screen(settings_screen, timeout_s)
	ids = sess.widget_map()
	category = _sound_category(ids)
	if ids[category].get("value") != checkbox_on:
		sess.click_widget(category)
		common.poll_until(lambda: sess.widget(category).get("value") == checkbox_on, timeout_s, poll_s)
	tick_id = _master_enabled(sess.widget_map())
	want = checkbox_off if muted else checkbox_on
	before = sess.widget(tick_id).get("value")
	clicked = before != want
	if clicked:
		sess.click_widget(tick_id)
		if not common.poll_until(lambda: sess.widget(tick_id).get("value") == want, timeout_s, poll_s):
			raise common.flow_error("{} did not change to '{}'".format(tick_id, want))
	if client_json:
		def persisted():
			try:
				return common.read_json_file(client_json).get("audio", {}).get("master_muted") == bool(muted)
			except (OSError, ValueError):
				return False
		if not common.poll_until(persisted, timeout_s, poll_s):
			raise common.flow_error("{} audio.master_muted is not {}".format(client_json, bool(muted)))
	sess.click_widget("settings.back")
	sess.wait_screen(main_menu, timeout_s)
	return {"previous_muted": before == checkbox_off, "muted": bool(muted), "clicked": clicked, "widget": tick_id,
	        "via": "settings"}


def set_audio_mute(sess, muted, client_json=None, timeout_s=default_timeout_s):
	"""Mute / unmute the master volume on a logged-in client through the bridge's set_audio, proven by its
	read-back (and by client.json when given - it is then saved with persist). Raises for a client that has
	no set_audio. Returns the set_master_mute shape with via set_audio."""
	try:
		before = sess.set_audio()
		reply = sess.set_audio(master_muted=bool(muted), persist=bool(client_json))
	except bridge.bridge_error as error:
		if "unknown cmd" in str(error):
			raise common.flow_error("this client has no set_audio and is logged in: pass leave_session=True to "
			                        "mute through the Settings screen instead (it logs the client out)")
		raise
	if reply.get("master_muted") is not bool(muted):
		raise common.flow_error("set_audio did not change master_muted to {} (reads {})".format(bool(muted), reply))
	if client_json:
		def persisted():
			try:
				return common.read_json_file(client_json).get("audio", {}).get("master_muted") == bool(muted)
			except (OSError, ValueError):
				return False
		if not common.poll_until(persisted, timeout_s, poll_s):
			raise common.flow_error("{} audio.master_muted is not {}".format(client_json, bool(muted)))
	return {"previous_muted": bool(before.get("master_muted")), "muted": bool(muted),
	        "clicked": before.get("master_muted") is not bool(muted), "widget": None, "via": "set_audio",
	        "audio": reply}


def read_config_mute(client_json, timeout_s=default_timeout_s):
	"""Prove a config mute from `audio.master_muted` in client.json alone (no UI), for a client that
	is not on the main menu (HB-0217). Returns the same shape as set_master_mute, with widget None."""
	def persisted():
		try:
			return common.read_json_file(client_json).get("audio", {}).get("master_muted") is True
		except (OSError, ValueError):
			return False
	if not common.poll_until(persisted, timeout_s, poll_s):
		raise common.flow_error("{} audio.master_muted is not true".format(client_json))
	return {"previous_muted": None, "muted": True, "clicked": False, "widget": None, "proof": "client.json"}


def prove_launch_mute(sess, mode, client_json, timeout_s=default_timeout_s):
	"""The mute proof for a freshly launched client. `mode` is "config" or "bridge" (running.mute_modes).

	On the main menu (or any screen that can walk back to it) the Settings tick is read back. On a
	logged-in start screen (charselect, charcreate) the tick is unreachable without logging out
	(HB-0158), so a config mute is proven from client.json, and a bridge mute is made with set_audio.
	"""
	if sess.screen() in logged_in_screens:
		if mode != "config":
			return set_audio_mute(sess, True, client_json, timeout_s)
		return read_config_mute(client_json, timeout_s)
	return set_master_mute(sess, True, client_json=client_json, timeout_s=timeout_s)


# --- account and login -----------------------------------------------------------------------------

def _fill(sess, widget_id, value, masked=False):
	"""Focus a text field by clicking it, replace its contents, and prove the value landed."""
	sess.click_widget(widget_id)
	sess.set_text(value)
	shown = sess.widget(widget_id).get("value")
	expected = "*" * len(value) if masked else value
	if shown != expected:
		raise common.flow_error("field {} reads {!r} after set_text".format(widget_id, "*" * len(shown or "") if masked else shown))


def create_account(sess, ident, login_log=None, timeout_s=default_timeout_s):
	"""Create the identity's account through the client's New Account page. Ends on the main menu."""
	go_to_main_menu(sess, timeout_s)
	sess.click_widget("main_menu.new_account")
	sess.wait_screen(account_create, timeout_s)
	_fill(sess, "account_create.account", ident.account)
	_fill(sess, "account_create.password", ident.password, masked=True)
	_fill(sess, "account_create.confirm", ident.password, masked=True)
	if ident.email:
		_fill(sess, "account_create.email", ident.email)
	watch = _watch(login_log)
	sess.click_widget("account_create.create")
	ok = resolve_request(sess, account_create, "account_create.create",
	                     lambda state, listing: state.get("screen") == main_menu, timeout_s)
	if not ok:
		reason = _refusal(watch, account_refusal_needles)
		sess.click_widget("account_create.cancel")
		sess.wait_screen(main_menu, timeout_s)
		raise common.flow_error("account {!r} was not created: {}".format(ident.account, reason or "refused (no log line)"))
	sess.wait_screen(main_menu, timeout_s)
	ident.mark(account_created=True)
	created = _refusal(watch, ("account created: '{}'".format(ident.account),))
	return {"account": ident.account, "log": created}


def log_in(sess, ident, login_log=None, timeout_s=default_timeout_s):
	"""Log the identity in from the main menu or the login screen (walking back first from anywhere
	else - including character select, which may hold ANOTHER account). Ends on character select."""
	if sess.screen() != login_screen:
		go_to_main_menu(sess, timeout_s)
		sess.click_widget("main_menu.login")
	sess.wait_screen(login_screen, timeout_s)
	sess.wait_widget("login.login")
	_fill(sess, "login.account", ident.account)
	_fill(sess, "login.password", ident.password, masked=True)
	watch = _watch(login_log)
	sess.click_widget("login.login")
	ok = resolve_request(sess, login_screen, "login.login",
	                     lambda state, listing: state.get("screen") == character_select, timeout_s)
	if not ok:
		raise common.flow_error("login as {!r} failed: {}".format(
			ident.account, _refusal(watch, login_refusal_needles) or "refused (no log line)"))
	sess.wait_screen(character_select, timeout_s)
	sess.wait_widget("char_select.log_out")
	return {"account": ident.account, "characters": character_names(sess)}


# --- characters --------------------------------------------------------------------------------------

def character_slots(sess, listing=None):
	"""[{slot, name, selected, widget_id}] for the filled character-select cards, by slot order."""
	slots = []
	for widget in (listing or sess.widgets()).get("widgets") or []:
		widget_id = widget.get("id", "")
		if widget_id.startswith("char_select.slot") and widget.get("label"):
			slots.append({"slot": int(widget_id[len("char_select.slot"):]), "name": widget["label"],
			              "selected": widget.get("value") == checkbox_on, "widget_id": widget_id,
			              "enabled": widget.get("enabled", True)})
	return sorted(slots, key=lambda entry: entry["slot"])


def character_names(sess, listing=None):
	return [entry["name"] for entry in character_slots(sess, listing)]


def _appearance_value(sess, row):
	return int(sess.widget("char_create.{}_next".format(row)).get("value"))


def create_character(sess, ident, name=None, preset=None, appearance=None, login_log=None,
                     timeout_s=default_timeout_s):
	"""Create a character on the logged-in identity's account. Ends on character select.

	name: from the identity (reserved there if new); preset: an archetype LABEL (case-insensitive,
	e.g. "Warrior") or index, default the first; appearance: {gender|skin|hair_style|hair_color|
	underwear: value}, stepped with the `_next` arrows (they wrap).
	"""
	sess.wait_screen(character_select, timeout_s)
	entry = ident.add_character(name) if name else next(
		(item for item in ident.characters if not item.get("created")), None) or ident.add_character()
	name = entry["name"]
	if name in character_names(sess):
		raise common.flow_error("{!r} is already on this account".format(name))
	sess.click_widget("char_select.new")
	sess.wait_screen(character_create, timeout_s)
	_fill(sess, "char_create.name", name)
	presets = [widget for widget in sess.widgets().get("widgets") or []
	           if widget.get("id", "").startswith("char_create.preset")]
	if not presets:
		raise common.flow_error("the character create page reports no archetype presets")
	chosen = presets[0]
	if isinstance(preset, int) or (isinstance(preset, str) and preset.isdigit()):
		chosen = next((widget for widget in presets if widget["id"] == "char_create.preset{}".format(int(preset))), None)
	elif preset:
		chosen = next((widget for widget in presets if (widget.get("label") or "").lower() == preset.lower()), None)
	if chosen is None:
		raise common.flow_error("no preset {!r}; the page offers {}".format(preset, [w.get("label") for w in presets]))
	sess.click_widget(chosen["id"])
	for row, value in (appearance or {}).items():
		for _ in range(max_stepper_clicks):
			if _appearance_value(sess, row) == int(value):
				break
			sess.click_widget("char_create.{}_next".format(row))
		else:
			raise common.flow_error("appearance {} never reached {}".format(row, value))
	create = sess.widget("char_create.create")
	if not create or not create.get("enabled"):
		raise common.flow_error("Create is disabled (empty name or unspent stat points)")
	watch = _watch(login_log)
	sess.click_widget("char_create.create")
	ok = resolve_request(sess, character_create, "char_create.cancel",
	                     lambda state, listing: state.get("screen") == character_select, timeout_s)
	if not ok:
		reason = _refusal(watch, character_refusal_needles)
		sess.click_widget("char_create.cancel")
		sess.wait_screen(character_select, timeout_s)
		raise common.flow_error("character {!r} was not created: {}".format(name, reason or "refused (no log line)"))
	sess.wait_screen(character_select, timeout_s)
	if not common.poll_until(lambda: name in character_names(sess), timeout_s, poll_s):
		raise common.flow_error("{!r} is not on character select after creation".format(name))
	created_line = _refusal(watch, ("character created: '{}'".format(name),))
	ident.mark_character(name, created=True, preset=chosen.get("label"))
	return {"name": name, "preset": chosen.get("label"), "log": created_line}


def select_character(sess, name, timeout_s=default_timeout_s):
	"""Select a character BY NAME (the card's drawn label) and confirm it is the selected one.

	Clicks at most once: a second click on the same card within the double-click window ENTERS the
	world (character_select_screen.cpp handle_page_click).
	"""
	sess.wait_screen(character_select, timeout_s)
	sess.wait_widget("char_select.log_out")
	slots = character_slots(sess)
	match = next((entry for entry in slots if entry["name"] == name), None)
	if match is None:
		raise common.flow_error("no character {!r} on this account (cards: {})".format(name, [s["name"] for s in slots]))
	if not match["selected"]:
		sess.click_widget(match["widget_id"])
	confirmed = common.poll_until(
		lambda: next((entry for entry in character_slots(sess) if entry["selected"]), {}).get("name") == name,
		timeout_s, poll_s)
	if not confirmed:
		raise common.flow_error("{!r} did not become the selected card".format(name))
	return {"name": name, "slot": match["slot"]}


def delete_character(sess, ident, name, login_log=None, timeout_s=default_timeout_s):
	"""Delete a character THIS tooling created (cleanup). Ends on character select."""
	if not ident.created_character(name):
		raise common.flow_error("refused: {!r} was not created by this tooling on identity {}".format(name, ident.path))
	select_character(sess, name, timeout_s)
	sess.click_widget("char_select.delete")
	sess.wait_widget("char_select.delete_yes")
	watch = _watch(login_log)
	sess.click_widget("char_select.delete_yes")
	ok = resolve_request(sess, character_select, "char_select.log_out",
	                     lambda state, listing: sess.widget("char_select.log_out", listing) is not None
	                     and sess.widget("char_select.log_out", listing).get("enabled")
	                     and name not in character_names(sess, listing), timeout_s)
	if not ok:
		reason = _refusal(watch, delete_refusal_needles)
		if sess.widget("char_select.delete_no"):
			sess.click_widget("char_select.delete_no")
		raise common.flow_error("{!r} was not deleted: {}".format(name, reason or "refused (no log line)"))
	ident.mark_character(name, created=False, deleted=True)
	return {"deleted": name}


# --- the world ---------------------------------------------------------------------------------------

def entry_conditions():
	"""What ends the wait after Start: the swap to the in-game screen, or a death on arrival."""
	return [bridge.event_condition("ui.screen", "screen_changed", {"to": in_game}),
	        bridge.window_condition("revive", "window_opened")]


def wait_spawned(sess, timeout_s=default_timeout_s):
	"""Poll get_state until `object_id` appears: in_game_screen reports it only once apply_spawn has
	run, so this holds whether the spawn lands before the screen swap (drained during the fade,
	HB-0156) or after it. Where the client reports `spawned` (the first world frame has run, so a probe
	is meaningful) that must be true too; a client without the field is judged by object_id alone.
	Returns the state."""
	def spawned():
		current = sess.state()
		return current if current.get("object_id") and current.get("spawned", True) else None
	state = common.poll_until(spawned, timeout_s, spawn_poll_s)
	if not state:
		raise common.flow_error("no spawn (get_state has no object_id) within {:g}s".format(timeout_s))
	return state


def enter_world(sess, name=None, login_log=None, timeout_ms=enter_world_timeout_ms):
	"""Select `name` (if given) and press Start; returns once the character has spawned.

	Ready = `ui.screen/screen_changed {to: in_game_screen}` (wait registered BEFORE the click, so it
	cannot be missed), then get_state `object_id` present (the spawn applied). `transitioning` and
	the `entering` window are not used as ready markers (HB-0159).
	"""
	if name:
		select_character(sess, name)
	watch = _watch(login_log)
	outcome, _ = sess.wait_for_then(entry_conditions(), timeout_ms, "widget_click", {"id": "char_select.start"})
	if not outcome.satisfied:
		reason = _refusal(watch, enter_refusal_needles)
		if sess.screen() == character_select:
			try:
				# the entering popup reports `entering.back` once the entry failed; then the refusal message
				back = sess.widget(entering_back)
				if back is not None and back.get("enabled", True):
					sess.click_widget(entering_back)
				# dismiss the refusal message so the page is usable again
				resolve_request(sess, character_select, "char_select.log_out", lambda state, listing: False)
			except common.flow_error:
				pass
		raise common.flow_error("did not enter the world within {} ms: {}".format(timeout_ms, reason or "no log line"))
	state = wait_spawned(sess)
	sess.wait_screen(in_game, default_timeout_s)
	if name and state.get("character") != name:
		raise common.flow_error("entered as {!r}, expected {!r}".format(state.get("character"), name))
	return {"character": state.get("character"), "object_id": state.get("object_id"),
	        "killed_on_entry": outcome.index == 1}


def switch_character(sess, timeout_s=default_timeout_s):
	"""In-world -> character select (Settings > System > Switch Character). The world saves the
	character as the session closes; server_logs.wait_logout_saved proves it."""
	sess.wait_screen(in_game, timeout_s)
	_open_world_settings(sess)
	sess.click_widget("settings.switch_character")
	sess.wait_screen(character_select, timeout_s)
	sess.wait_widget("char_select.log_out")
	return {"screen": character_select}


# Where log_out may end (HB-0625): the main menu (default) or one step further, the login screen.
log_out_targets = ("main_menu", "login")


def log_out(sess, timeout_s=default_timeout_s, to="main_menu"):
	"""Leave the world or character select for the main menu, or with to='login' go on to the login screen."""
	if to not in log_out_targets:
		raise common.flow_error("logout 'to' must be one of {} (got {!r})".format(", ".join(log_out_targets), to))
	screen = go_to_main_menu(sess, timeout_s).get("screen")
	if to == "login":
		sess.click_widget("main_menu.login")
		screen = sess.wait_screen(login_screen, timeout_s).get("screen", login_screen)
	return {"screen": screen}


def quit_client(sess, process=None, timeout_s=default_timeout_s):
	"""Main menu > Exit. With the client_process, waits for the process to end (stops it if it
	does not). Without one, waits for the bridge to stop answering."""
	go_to_main_menu(sess, timeout_s)
	sess.click_widget("main_menu.exit")
	if process is not None:
		if not process.wait_exit(timeout_s):
			process.stop()
			return {"exited": False, "stopped": True}
		return {"exited": True}

	def gone():
		try:
			sess.ping()
			return False
		except bridge.bridge_error:
			return True
	return {"exited": bool(common.poll_until(gone, timeout_s, poll_s * 5))}
