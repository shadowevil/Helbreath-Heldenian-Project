"""Menu and login flows for this client: main menu, login, character select, enter the world.

Every flow drives the client by the widget names `widgets` reports (login.name, login.ok, select_character.slot1,
...), never by remembered pixels, and ends by PROVING where it landed (get_state screen / character). A flow
that cannot reach its goal raises common.flow_error saying where it stopped.

Credentials come from an identity (identity.test_account() for a gitignored local test account, identity.from_env()
for HB_TEST_ACCOUNT / HB_TEST_PASSWORD, or a scratch identity file). The password is typed into a masked field
and is never returned, logged or echoed; a refusal is reported by the overlay that came up, not by its text.

Not ported yet (they raise flow_error saying so): account and character creation, character delete, the mute and
logout flows, quit. They need widgets this client does not report or an in-world settings path.
"""

import time

from . import common

# This client's screen ids (get_state "screen" is the screen class name).
main_menu = "Screen_MainMenu"
login_screen = "Screen_Login"
account_create = "Screen_CreateAccount"
character_select = "Screen_SelectCharacter"
character_create = "Screen_CreateNewCharacter"
in_game = "Screen_OnGame"
exit_screen = "Screen_Quit"

# The overlays a request raises: busy ones clear by themselves, message ones need a click on their ok.
busy_overlays = ("Overlay_Connecting", "Overlay_WaitingResponse", "Overlay_WaitInitData")
message_overlays = {"Overlay_LogResMsg": "log_res_msg.ok", "Overlay_VersionNotMatch": "version_not_match.ok"}
force_login_overlay = "Overlay_QueryForceLogin"

default_timeout_s = 20.0
enter_world_timeout_s = 45.0
poll_s = 0.1
max_navigation_steps = 8
log_out_targets = ("main_menu", "login")
character_slots = 4

# The widget that leaves each menu screen toward the main menu.
leave_widgets = {
	login_screen: "login.cancel",
	account_create: "create_account.cancel",
	character_create: "create_character.cancel",
	character_select: "select_character.exit",
}


def not_ported(what):
	raise common.flow_error("{} is not ported to this client yet".format(what))


def _wait_idle(sess, timeout_s):
	"""The state once no screen transition is running."""
	state = common.poll_until(lambda: (lambda s: s if not s.get("transitioning") else None)(sess.state()), timeout_s, poll_s)
	if not state:
		raise common.flow_error("the client is still transitioning after {:g}s".format(timeout_s))
	return state


def go_to_main_menu(sess, timeout_s=default_timeout_s):
	"""Walk back to the main menu from a menu screen. A message overlay in the way is dismissed. The world is
	not left: that needs the in-game menu, which this client does not report yet."""
	for _ in range(max_navigation_steps):
		state = sess.state()
		screen = state.get("screen")
		if state.get("transitioning"):
			time.sleep(poll_s)
			continue
		if screen == main_menu and not state.get("overlay"):
			return sess.wait_screen(main_menu, timeout_s)
		if state.get("overlay") in message_overlays:
			sess.click_widget(message_overlays[state["overlay"]])
			time.sleep(poll_s * 5)
			continue
		if screen == in_game:
			not_ported("leaving the world")
		if screen in leave_widgets:
			sess.click_widget(leave_widgets[screen])
			common.poll_until(lambda: sess.state().get("screen") != screen, timeout_s, poll_s)
		else:
			time.sleep(poll_s * 5)  # splash / loading / quit: it moves on by itself
	raise common.flow_error("could not reach the main menu (stuck on '{}')".format(sess.screen()))


def _fill(sess, widget_id, value, masked=False):
	"""Focus a text field by its widget name and set its text. A plain field is read back (its length) because a
	typed first character has been lost before; a masked one cannot be, so only its focus is proven."""
	for attempt in range(2):
		sess.click_widget(widget_id)
		state = sess.state()
		sess.set_text(value)
		state = sess.state()
		if masked:
			return
		if state.get("focus_text_length") == len(value):
			return
	raise common.flow_error("could not type into {} (the field holds {} characters, wanted {})".format(
		widget_id, state.get("focus_text_length"), len(value)))


def _dismiss_message(sess, overlay):
	try:
		sess.click_widget(message_overlays[overlay])
	except common.flow_error:
		pass


def log_in(sess, ident, timeout_s=default_timeout_s, force=False):
	"""Main menu or login screen -> character select, with the identity's account. Returns {account, slots, screen}.
	A refusal (a message overlay) is dismissed and raised; an 'already logged in' prompt is refused unless force."""
	state = _wait_idle(sess, timeout_s)
	if state.get("overlay") in message_overlays:  # a leftover message from an earlier attempt
		_dismiss_message(sess, state["overlay"])
		time.sleep(poll_s * 5)
		state = _wait_idle(sess, timeout_s)
	if state.get("screen") == main_menu:
		sess.click_widget("main_menu.login")
		sess.wait_screen(login_screen, timeout_s)
	elif state.get("screen") != login_screen:
		raise common.flow_error("log_in starts on the main menu or the login screen, not '{}'".format(state.get("screen")))
	_fill(sess, "login.name", ident.account)
	_fill(sess, "login.password", ident.password, masked=True)
	sess.click_widget("login.ok")
	deadline = time.monotonic() + timeout_s
	while time.monotonic() < deadline:
		state = sess.state()
		overlay = state.get("overlay")
		if state.get("screen") == character_select and not state.get("transitioning") and not overlay:
			return {"account": ident.account, "screen": character_select, "slots": character_slot_names(sess)}
		if overlay in message_overlays:
			_dismiss_message(sess, overlay)
			raise common.flow_error("login was refused ({}); the screen is '{}'".format(overlay, state.get("screen")))
		if overlay == force_login_overlay:
			if not force:
				sess.click_widget("query_force_login.no")
				raise common.flow_error("the account is already logged in elsewhere (pass force to take it over)")
			sess.click_widget("query_force_login.yes")
		time.sleep(poll_s)
	raise common.flow_error("login did not reach character select within {:g}s (at '{}')".format(timeout_s, sess.screen()))


def character_slot_names(sess):
	"""The slot widgets character select reports, in slot order: [{slot, widget, enabled}]."""
	ids = sess.widget_map()
	rows = []
	for number in range(1, character_slots + 1):
		row = ids.get("select_character.slot{}".format(number))
		if row is not None:
			rows.append({"slot": number, "widget": row["id"], "enabled": row.get("enabled", True)})
	return rows


def slot_for(ident, name=None, slot=None):
	"""The slot number to click: `slot` as given, else the position of `name` among the identity's characters
	(slot order), else slot 1."""
	if slot is not None:
		number = int(slot)
	elif name and ident is not None:
		names = [entry["name"] for entry in ident.characters]
		if name not in names:
			raise common.flow_error("character '{}' is not listed on the identity (it has: {})".format(name, ", ".join(names) or "none"))
		number = names.index(name) + 1
	else:
		number = 1
	if not 1 <= number <= character_slots:
		raise common.flow_error("slot must be 1..{}".format(character_slots))
	return number


def select_character(sess, name=None, timeout_s=default_timeout_s, ident=None, slot=None):
	"""Click a character slot on the select screen. Returns {slot, character}."""
	sess.wait_screen(character_select, timeout_s)
	number = slot_for(ident, name, slot)
	sess.click_widget("select_character.slot{}".format(number))
	return {"slot": number, "character": name}


def enter_world(sess, name=None, timeout_s=enter_world_timeout_s, ident=None, slot=None):
	"""Character select -> in the world: select the slot, press Enter, wait for Screen_OnGame and a character.
	When `name` is given the character that arrived must be that one."""
	picked = select_character(sess, name, default_timeout_s, ident=ident, slot=slot)
	sess.click_widget("select_character.enter")
	deadline = time.monotonic() + timeout_s
	while time.monotonic() < deadline:
		state = sess.state()
		overlay = state.get("overlay")
		if overlay in message_overlays:
			_dismiss_message(sess, overlay)
			raise common.flow_error("entering the world was refused ({})".format(overlay))
		if state.get("screen") == in_game and state.get("in_world") and not state.get("transitioning"):
			if name and state.get("character") != name:
				raise common.flow_error("entered as '{}', not '{}'".format(state.get("character"), name))
			return {"character": state.get("character"), "map": state.get("map"),
			        "player_tile": [state.get("player_x"), state.get("player_y")], "slot": picked["slot"]}
		time.sleep(poll_s)
	raise common.flow_error("did not reach the world within {:g}s (at '{}')".format(timeout_s, sess.screen()))


# --- not ported ------------------------------------------------------------------------------------------------

def create_account(sess, ident, timeout_s=default_timeout_s):
	not_ported("create_account")


def create_character(sess, ident, name=None, preset=None, appearance=None, timeout_s=default_timeout_s):
	not_ported("create_character")


def delete_character(sess, ident, name, timeout_s=default_timeout_s):
	not_ported("delete_character")


def set_master_mute(sess, muted, client_json=None, leave_session=False, timeout_s=default_timeout_s):
	not_ported("the mute flow (use hb_set_audio)")


def prove_launch_mute(sess, mode, client_json):
	not_ported("the launch mute proof (use hb_set_audio)")


def switch_character(sess, timeout_s=default_timeout_s):
	not_ported("switch_character")


def log_out(sess, timeout_s=default_timeout_s, to="main_menu"):
	not_ported("log_out")


def quit_client(sess, timeout_s=default_timeout_s):
	not_ported("quit_client")
