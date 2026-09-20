#include "FormulaEngine.h"
#include "Log.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>
#include <stdexcept>

namespace hb::shared
{

// ============================================================================
// Tokenizer
// ============================================================================

namespace
{

enum class token_type { number, identifier, plus, minus, star, slash, lparen, rparen, comma, end };

struct token
{
	token_type type = token_type::end;
	std::string text;
	double num_value = 0.0;
};

std::vector<token> tokenize(const std::string& expr)
{
	std::vector<token> tokens;
	size_t i = 0;
	while (i < expr.size())
	{
		char c = expr[i];
		if (std::isspace(static_cast<unsigned char>(c))) { ++i; continue; }

		if (std::isdigit(static_cast<unsigned char>(c)) || (c == '.' && i + 1 < expr.size() && std::isdigit(static_cast<unsigned char>(expr[i + 1]))))
		{
			size_t start = i;
			while (i < expr.size() && (std::isdigit(static_cast<unsigned char>(expr[i])) || expr[i] == '.'))
				++i;
			std::string num_str = expr.substr(start, i - start);
			token t;
			t.type = token_type::number;
			t.text = num_str;
			t.num_value = std::stod(num_str);
			tokens.push_back(std::move(t));
			continue;
		}

		if (std::isalpha(static_cast<unsigned char>(c)) || c == '_')
		{
			size_t start = i;
			while (i < expr.size() && (std::isalnum(static_cast<unsigned char>(expr[i])) || expr[i] == '_'))
				++i;
			token t;
			t.type = token_type::identifier;
			t.text = expr.substr(start, i - start);
			tokens.push_back(std::move(t));
			continue;
		}

		token t;
		switch (c)
		{
		case '+': t.type = token_type::plus; break;
		case '-': t.type = token_type::minus; break;
		case '*': t.type = token_type::star; break;
		case '/': t.type = token_type::slash; break;
		case '(': t.type = token_type::lparen; break;
		case ')': t.type = token_type::rparen; break;
		case ',': t.type = token_type::comma; break;
		default:
			throw std::runtime_error(std::string("Unexpected character '") + c + "' in expression");
		}
		t.text = std::string(1, c);
		tokens.push_back(std::move(t));
		++i;
	}
	tokens.push_back(token{token_type::end, "", 0.0});
	return tokens;
}

// ============================================================================
// Recursive descent parser
// ============================================================================

class expression_parser
{
public:
	explicit expression_parser(const std::vector<token>& tokens)
		: m_tokens(tokens), m_pos(0) {}

	ast_ptr parse()
	{
		auto node = parse_additive();
		if (current().type != token_type::end)
			throw std::runtime_error("Unexpected token: " + current().text);
		return node;
	}

private:
	const token& current() const { return m_tokens[m_pos]; }
	const token& advance() { return m_tokens[m_pos++]; }

	ast_ptr parse_additive()
	{
		auto left = parse_multiplicative();
		while (current().type == token_type::plus || current().type == token_type::minus)
		{
			char op = (current().type == token_type::plus) ? '+' : '-';
			advance();
			auto right = parse_multiplicative();
			auto node = std::make_unique<ast_node>();
			node->type = ast_type::binary_op;
			node->op = op;
			node->children.push_back(std::move(left));
			node->children.push_back(std::move(right));
			left = std::move(node);
		}
		return left;
	}

	ast_ptr parse_multiplicative()
	{
		auto left = parse_unary();
		while (current().type == token_type::star || current().type == token_type::slash)
		{
			char op = (current().type == token_type::star) ? '*' : '/';
			advance();
			auto right = parse_unary();
			auto node = std::make_unique<ast_node>();
			node->type = ast_type::binary_op;
			node->op = op;
			node->children.push_back(std::move(left));
			node->children.push_back(std::move(right));
			left = std::move(node);
		}
		return left;
	}

	ast_ptr parse_unary()
	{
		if (current().type == token_type::minus)
		{
			advance();
			auto operand = parse_unary();
			auto node = std::make_unique<ast_node>();
			node->type = ast_type::unary_neg;
			node->children.push_back(std::move(operand));
			return node;
		}
		return parse_primary();
	}

	ast_ptr parse_primary()
	{
		if (current().type == token_type::number)
		{
			auto node = std::make_unique<ast_node>();
			node->type = ast_type::literal;
			node->value = current().num_value;
			advance();
			return node;
		}

		if (current().type == token_type::identifier)
		{
			std::string name = current().text;
			advance();

			// Function call: identifier '(' args ')'
			if (current().type == token_type::lparen)
			{
				advance(); // consume '('
				auto node = std::make_unique<ast_node>();
				node->type = ast_type::func_call;
				node->name = name;

				if (current().type != token_type::rparen)
				{
					node->children.push_back(parse_additive());
					while (current().type == token_type::comma)
					{
						advance(); // consume ','
						node->children.push_back(parse_additive());
					}
				}

				if (current().type != token_type::rparen)
					throw std::runtime_error("Expected ')' after function arguments");
				advance(); // consume ')'
				return node;
			}

			// Plain variable
			auto node = std::make_unique<ast_node>();
			node->type = ast_type::variable;
			node->name = name;
			return node;
		}

		if (current().type == token_type::lparen)
		{
			advance(); // consume '('
			auto node = parse_additive();
			if (current().type != token_type::rparen)
				throw std::runtime_error("Expected ')'");
			advance(); // consume ')'
			return node;
		}

		throw std::runtime_error("Unexpected token: " + current().text);
	}

	const std::vector<token>& m_tokens;
	size_t m_pos;
};

// ============================================================================
// Serialization helpers
// ============================================================================

void write_string(std::vector<uint8_t>& buf, const std::string& s)
{
	uint16_t len = static_cast<uint16_t>(s.size());
	buf.push_back(static_cast<uint8_t>(len & 0xFF));
	buf.push_back(static_cast<uint8_t>((len >> 8) & 0xFF));
	buf.insert(buf.end(), s.begin(), s.end());
}

template <typename T>
void write_val(std::vector<uint8_t>& buf, T val)
{
	const auto* bytes = reinterpret_cast<const uint8_t*>(&val);
	buf.insert(buf.end(), bytes, bytes + sizeof(T));
}

bool read_string(const uint8_t*& ptr, size_t& remaining, std::string& out)
{
	if (remaining < 2) return false;
	uint16_t len = static_cast<uint16_t>(ptr[0]) | (static_cast<uint16_t>(ptr[1]) << 8);
	ptr += 2;
	remaining -= 2;
	if (remaining < len) return false;
	out.assign(reinterpret_cast<const char*>(ptr), len);
	ptr += len;
	remaining -= len;
	return true;
}

template <typename T>
bool read_val(const uint8_t*& ptr, size_t& remaining, T& out)
{
	if (remaining < sizeof(T)) return false;
	std::memcpy(&out, ptr, sizeof(T));
	ptr += sizeof(T);
	remaining -= sizeof(T);
	return true;
}

} // anonymous namespace

// ============================================================================
// formula_engine implementation
// ============================================================================

void formula_engine::clear()
{
	m_formulas.clear();
	m_scaling_profiles.clear();
}

bool formula_engine::add_formula(const std::string& id, const std::string& expression,
	const std::string& description)
{
	try
	{
		auto tokens = tokenize(expression);
		expression_parser parser(tokens);
		auto root = parser.parse();

		parsed_formula f;
		f.formula_id = id;
		f.expression = expression;
		f.description = description;
		collect_variables(root.get(), f.required_vars);
		f.root = std::move(root);
		m_formulas[id] = std::move(f);
		return true;
	}
	catch (const std::exception& e)
	{
		hb::logger::error("Formula '{}' parse error: {} (expression: {})", id, e.what(), expression);
		return false;
	}
}

void formula_engine::add_scaling_profile(const scaling_profile& profile)
{
	m_scaling_profiles[profile.profile_id] = profile;
}

bool formula_engine::has_formula(const std::string& formula_id) const
{
	return m_formulas.find(formula_id) != m_formulas.end();
}

size_t formula_engine::formula_count() const
{
	return m_formulas.size();
}

int formula_engine::evaluate(const std::string& formula_id, const stat_map& stats) const
{
	auto it = m_formulas.find(formula_id);
	if (it == m_formulas.end())
		return 0;

	if (!it->second.root)
		return 0;

	double result = evaluate_node(it->second.root.get(), stats, 0);
	return static_cast<int>(result);
}

double formula_engine::evaluate_node(const ast_node* node, const stat_map& stats, int depth) const
{
	if (!node) return 0.0;
	if (depth > max_depth)
		throw std::runtime_error("Formula evaluation depth exceeded (circular reference?)");

	switch (node->type)
	{
	case ast_type::literal:
		return node->value;

	case ast_type::variable:
	{
		// 1. Check stat_map first
		auto it = stats.find(node->name);
		if (it != stats.end())
			return it->second;

		// 2. Check if it's a formula cross-reference
		auto fit = m_formulas.find(node->name);
		if (fit != m_formulas.end() && fit->second.root)
			return evaluate_node(fit->second.root.get(), stats, depth + 1);

		// 3. Not found — return 0
		return 0.0;
	}

	case ast_type::binary_op:
	{
		double left = evaluate_node(node->children[0].get(), stats, depth);
		double right = evaluate_node(node->children[1].get(), stats, depth);
		switch (node->op)
		{
		case '+': return left + right;
		case '-': return left - right;
		case '*': return left * right;
		case '/': return (right != 0.0) ? left / right : 0.0;
		}
		return 0.0;
	}

	case ast_type::unary_neg:
		return -evaluate_node(node->children[0].get(), stats, depth);

	case ast_type::func_call:
	{
		const auto& name = node->name;

		// Built-in: trunc(x)
		if (name == "trunc")
		{
			if (node->children.size() != 1)
				throw std::runtime_error("trunc() requires exactly 1 argument");
			double val = evaluate_node(node->children[0].get(), stats, depth);
			return static_cast<double>(static_cast<int64_t>(val));
		}

		// Built-in: floor(x)
		if (name == "floor")
		{
			if (node->children.size() != 1)
				throw std::runtime_error("floor() requires exactly 1 argument");
			return std::floor(evaluate_node(node->children[0].get(), stats, depth));
		}

		// Built-in: ceil(x)
		if (name == "ceil")
		{
			if (node->children.size() != 1)
				throw std::runtime_error("ceil() requires exactly 1 argument");
			return std::ceil(evaluate_node(node->children[0].get(), stats, depth));
		}

		// Built-in: round(x)
		if (name == "round")
		{
			if (node->children.size() != 1)
				throw std::runtime_error("round() requires exactly 1 argument");
			return std::round(evaluate_node(node->children[0].get(), stats, depth));
		}

		// Built-in: abs(x)
		if (name == "abs")
		{
			if (node->children.size() != 1)
				throw std::runtime_error("abs() requires exactly 1 argument");
			return std::abs(evaluate_node(node->children[0].get(), stats, depth));
		}

		// Built-in: sqrt(x)
		if (name == "sqrt")
		{
			if (node->children.size() != 1)
				throw std::runtime_error("sqrt() requires exactly 1 argument");
			return std::sqrt(evaluate_node(node->children[0].get(), stats, depth));
		}

		// Built-in: log(x) — natural logarithm
		if (name == "log")
		{
			if (node->children.size() != 1)
				throw std::runtime_error("log() requires exactly 1 argument");
			return std::log(evaluate_node(node->children[0].get(), stats, depth));
		}

		// Built-in: log2(x)
		if (name == "log2")
		{
			if (node->children.size() != 1)
				throw std::runtime_error("log2() requires exactly 1 argument");
			return std::log2(evaluate_node(node->children[0].get(), stats, depth));
		}

		// Built-in: log10(x)
		if (name == "log10")
		{
			if (node->children.size() != 1)
				throw std::runtime_error("log10() requires exactly 1 argument");
			return std::log10(evaluate_node(node->children[0].get(), stats, depth));
		}

		// Built-in: pow(base, exp)
		if (name == "pow")
		{
			if (node->children.size() != 2)
				throw std::runtime_error("pow() requires exactly 2 arguments");
			double base = evaluate_node(node->children[0].get(), stats, depth);
			double exp = evaluate_node(node->children[1].get(), stats, depth);
			return std::pow(base, exp);
		}

		// Built-in: clamp(value, lo, hi)
		if (name == "clamp")
		{
			if (node->children.size() != 3)
				throw std::runtime_error("clamp() requires exactly 3 arguments");
			double val = evaluate_node(node->children[0].get(), stats, depth);
			double lo = evaluate_node(node->children[1].get(), stats, depth);
			double hi = evaluate_node(node->children[2].get(), stats, depth);
			return std::clamp(val, lo, hi);
		}

		// Built-in: min(a, b)
		if (name == "min")
		{
			if (node->children.size() != 2)
				throw std::runtime_error("min() requires exactly 2 arguments");
			double a = evaluate_node(node->children[0].get(), stats, depth);
			double b = evaluate_node(node->children[1].get(), stats, depth);
			return (a < b) ? a : b;
		}

		// Built-in: max(a, b)
		if (name == "max")
		{
			if (node->children.size() != 2)
				throw std::runtime_error("max() requires exactly 2 arguments");
			double a = evaluate_node(node->children[0].get(), stats, depth);
			double b = evaluate_node(node->children[1].get(), stats, depth);
			return (a > b) ? a : b;
		}

		// Built-in: sum(start, end, body) — loop variable is 'i'
		if (name == "sum")
		{
			if (node->children.size() != 3)
				throw std::runtime_error("sum() requires exactly 3 arguments");
			int start = static_cast<int>(evaluate_node(node->children[0].get(), stats, depth));
			int end = static_cast<int>(evaluate_node(node->children[1].get(), stats, depth));
			double total = 0.0;
			stat_map local_stats = stats;
			for (int i = start; i <= end; ++i)
			{
				local_stats["i"] = static_cast<double>(i);
				total += evaluate_node(node->children[2].get(), local_stats, depth + 1);
			}
			return total;
		}

		// Scaling profile function: profile_name(value)
		auto pit = m_scaling_profiles.find(name);
		if (pit != m_scaling_profiles.end())
		{
			if (node->children.size() != 1)
				throw std::runtime_error("Scaling profile '" + name + "' requires exactly 1 argument");
			double val = evaluate_node(node->children[0].get(), stats, depth);
			return apply_scaling(name, val);
		}

		hb::logger::warn("Unknown function '{}', treating as linear passthrough", name);
		if (!node->children.empty())
			return evaluate_node(node->children[0].get(), stats, depth);
		return 0.0;
	}
	}

	return 0.0;
}

double formula_engine::apply_scaling(const std::string& profile_id, double value) const
{
	auto it = m_scaling_profiles.find(profile_id);
	if (it == m_scaling_profiles.end())
		return value;

	int int_val = static_cast<int>(value);
	double result = 0.0;

	for (const auto& bracket : it->second.brackets)
	{
		if (int_val < bracket.bracket_min) break;
		int effective_max = std::min(int_val, bracket.bracket_max);
		int points_in_bracket = effective_max - bracket.bracket_min + 1;
		if (points_in_bracket > 0)
			result += points_in_bracket * bracket.multiplier;
	}

	return result;
}

void formula_engine::collect_variables(const ast_node* node, std::unordered_set<std::string>& vars) const
{
	if (!node) return;

	if (node->type == ast_type::variable)
	{
		vars.insert(node->name);
		return;
	}

	if (node->type == ast_type::func_call)
	{
		// For sum(), the body uses 'i' as loop variable — don't count as required
		if (node->name == "sum" && node->children.size() == 3)
		{
			collect_variables(node->children[0].get(), vars);
			collect_variables(node->children[1].get(), vars);
			// Body may reference 'i' — collect but remove 'i' after
			std::unordered_set<std::string> body_vars;
			collect_variables(node->children[2].get(), body_vars);
			body_vars.erase("i");
			vars.insert(body_vars.begin(), body_vars.end());
			return;
		}
	}

	for (const auto& child : node->children)
		collect_variables(child.get(), vars);
}

// ============================================================================
// Validation
// ============================================================================

validation_result formula_engine::validate() const
{
	validation_result vr;

	// 1. Parse check — any formula with null AST root
	for (const auto& [id, f] : m_formulas)
	{
		if (!f.root)
		{
			vr.success = false;
			std::string msg = "Formula '" + id + "' has no valid AST (parse error)";
			vr.errors.push_back(msg);
			hb::logger::error("{}", msg);
		}
	}

	// 2. Required formulas exist
	static const std::vector<std::string> required = {
		"max_hp", "max_mp", "max_sp", "max_load", "level_exp"
	};
	for (const auto& req : required)
	{
		if (m_formulas.find(req) == m_formulas.end())
		{
			vr.success = false;
			std::string msg = "Required formula '" + req + "' is missing";
			vr.errors.push_back(msg);
			hb::logger::error("{}", msg);
		}
	}

	// 3. Circular reference detection via DFS
	for (const auto& [id, f] : m_formulas)
	{
		std::unordered_set<std::string> visited;
		std::vector<std::string> stack;
		stack.push_back(id);
		bool circular = false;

		while (!stack.empty() && !circular)
		{
			std::string current = stack.back();
			stack.pop_back();

			if (visited.count(current) && current == id && visited.size() > 0)
			{
				circular = true;
				break;
			}
			visited.insert(current);

			auto fit = m_formulas.find(current);
			if (fit == m_formulas.end()) continue;

			for (const auto& var : fit->second.required_vars)
			{
				if (var == id && current != id)
				{
					circular = true;
					break;
				}
				if (m_formulas.count(var) && !visited.count(var))
					stack.push_back(var);
			}
		}

		if (circular)
		{
			vr.success = false;
			std::string msg = "Circular reference detected involving formula '" + id + "'";
			vr.errors.push_back(msg);
			hb::logger::error("{}", msg);
		}
	}

	// 4. Arg count check for built-in functions + unknown variable warnings
	for (const auto& [id, f] : m_formulas)
	{
		if (!f.root) continue;

		// Walk AST checking function arg counts
		std::vector<const ast_node*> work;
		work.push_back(f.root.get());
		while (!work.empty())
		{
			const ast_node* n = work.back();
			work.pop_back();
			if (!n) continue;

			if (n->type == ast_type::func_call)
			{
				size_t argc = n->children.size();
				if (n->name == "sum" && argc != 3)
				{
					vr.success = false;
					std::string msg = "Formula '" + id + "': sum() requires 3 args, got " + std::to_string(argc);
					vr.errors.push_back(msg);
					hb::logger::error("{}", msg);
				}
				else if ((n->name == "min" || n->name == "max") && argc != 2)
				{
					vr.success = false;
					std::string msg = "Formula '" + id + "': " + n->name + "() requires 2 args, got " + std::to_string(argc);
					vr.errors.push_back(msg);
					hb::logger::error("{}", msg);
				}
				else if ((n->name == "trunc" || n->name == "floor" || n->name == "ceil" || n->name == "round" || n->name == "abs" || n->name == "sqrt" || n->name == "log" || n->name == "log2" || n->name == "log10") && argc != 1)
				{
					vr.success = false;
					std::string msg = "Formula '" + id + "': " + n->name + "() requires 1 arg, got " + std::to_string(argc);
					vr.errors.push_back(msg);
					hb::logger::error("{}", msg);
				}
				else if (n->name == "pow" && argc != 2)
				{
					vr.success = false;
					std::string msg = "Formula '" + id + "': pow() requires 2 args, got " + std::to_string(argc);
					vr.errors.push_back(msg);
					hb::logger::error("{}", msg);
				}
				else if (n->name == "clamp" && argc != 3)
				{
					vr.success = false;
					std::string msg = "Formula '" + id + "': clamp() requires 3 args, got " + std::to_string(argc);
					vr.errors.push_back(msg);
					hb::logger::error("{}", msg);
				}
			}

			for (const auto& child : n->children)
				work.push_back(child.get());
		}

		// Check for unknown variables (not a known stat pattern and not a formula cross-ref)
		for (const auto& var : f.required_vars)
		{
			if (m_formulas.count(var)) continue; // valid cross-reference
			// 'i' is a loop variable for sum()
			if (var == "i") continue;
			// All other variables are expected to come from stat_map at runtime — just note them
		}
	}

	// 5. Unknown scaling profiles referenced as functions
	for (const auto& [id, f] : m_formulas)
	{
		if (!f.root) continue;

		std::vector<const ast_node*> work;
		work.push_back(f.root.get());
		while (!work.empty())
		{
			const ast_node* n = work.back();
			work.pop_back();
			if (!n) continue;

			if (n->type == ast_type::func_call)
			{
				// Check if it's a known built-in or scaling profile
				if (n->name != "sum" && n->name != "min" && n->name != "max" && n->name != "trunc" && n->name != "floor" && n->name != "ceil" && n->name != "round" && n->name != "abs" && n->name != "sqrt" && n->name != "log" && n->name != "log2" && n->name != "log10" && n->name != "pow" && n->name != "clamp")
				{
					if (!m_scaling_profiles.count(n->name))
					{
						std::string msg = "Formula '" + id + "': unknown function '" + n->name + "', will default to linear passthrough";
						vr.warnings.push_back(msg);
						hb::logger::warn("{}", msg);
					}
				}
			}

			for (const auto& child : n->children)
				work.push_back(child.get());
		}
	}

	// 6. Dry-run evaluation with sample data
	if (vr.success)
	{
		stat_map sample;
		sample["vit"] = 30;
		sample["str"] = 20;
		sample["dex"] = 20;
		sample["int"] = 15;
		sample["mag"] = 25;
		sample["chr"] = 10;
		sample["level"] = 50;
		sample["angelic_str"] = 0;
		sample["angelic_mag"] = 0;
		sample["angelic_int"] = 0;
		sample["angelic_dex"] = 0;
		sample["total_stats"] = 120;
		sample["base_stat_value"] = 10;
		sample["max_creation_stat_value"] = 4;
		sample["max_level"] = 180;

		for (const auto& [id, f] : m_formulas)
		{
			if (!f.root) continue;
			try
			{
				double result = evaluate_node(f.root.get(), sample, 0);
				if (std::isnan(result) || std::isinf(result))
				{
					vr.success = false;
					std::string msg = "Formula '" + id + "' dry-run produced NaN/Inf";
					vr.errors.push_back(msg);
					hb::logger::error("{}", msg);
				}
			}
			catch (const std::exception& e)
			{
				vr.success = false;
				std::string msg = "Formula '" + id + "' dry-run failed: " + e.what();
				vr.errors.push_back(msg);
				hb::logger::error("{}", msg);
			}
		}
	}

	if (vr.success)
		hb::logger::log("- {} formulas (validated, {} warnings)",
			m_formulas.size(), vr.warnings.size());

	return vr;
}

// ============================================================================
// Serialization — v2 binary format for client cache
// ============================================================================
//
// Format:
// [uint8_t version = 2]
// [uint32_t formula_count]
//   Per formula: [uint16_t id_len][chars] [uint16_t expr_len][chars] [uint16_t desc_len][chars]
// [uint32_t profile_count]
//   Per profile: [uint16_t id_len][chars] [uint16_t bracket_count]
//     Per bracket: [int32_t min][int32_t max][double multiplier]

std::vector<uint8_t> formula_engine::serialize() const
{
	std::vector<uint8_t> buf;
	buf.reserve(4096);

	// Version
	write_val(buf, static_cast<uint8_t>(2));

	// Formulas
	write_val(buf, static_cast<uint32_t>(m_formulas.size()));
	for (const auto& [id, f] : m_formulas)
	{
		write_string(buf, f.formula_id);
		write_string(buf, f.expression);
		write_string(buf, f.description);
	}

	// Scaling profiles
	write_val(buf, static_cast<uint32_t>(m_scaling_profiles.size()));
	for (const auto& [id, profile] : m_scaling_profiles)
	{
		write_string(buf, profile.profile_id);
		write_val(buf, static_cast<uint16_t>(profile.brackets.size()));
		for (const auto& bracket : profile.brackets)
		{
			write_val(buf, static_cast<int32_t>(bracket.bracket_min));
			write_val(buf, static_cast<int32_t>(bracket.bracket_max));
			write_val(buf, bracket.multiplier);
		}
	}

	return buf;
}

bool formula_engine::deserialize(const uint8_t* data, size_t size)
{
	clear();

	const uint8_t* ptr = data;
	size_t remaining = size;

	// Version
	uint8_t version = 0;
	if (!read_val(ptr, remaining, version)) return false;
	if (version != 2) return false;

	// Formulas
	uint32_t formula_count = 0;
	if (!read_val(ptr, remaining, formula_count)) return false;

	for (uint32_t i = 0; i < formula_count; ++i)
	{
		std::string id, expr, desc;
		if (!read_string(ptr, remaining, id)) return false;
		if (!read_string(ptr, remaining, expr)) return false;
		if (!read_string(ptr, remaining, desc)) return false;

		if (!add_formula(id, expr, desc))
			return false;
	}

	// Scaling profiles
	uint32_t profile_count = 0;
	if (!read_val(ptr, remaining, profile_count)) return false;

	for (uint32_t i = 0; i < profile_count; ++i)
	{
		scaling_profile profile;
		if (!read_string(ptr, remaining, profile.profile_id)) return false;

		uint16_t bracket_count = 0;
		if (!read_val(ptr, remaining, bracket_count)) return false;

		profile.brackets.resize(bracket_count);
		for (uint16_t b = 0; b < bracket_count; ++b)
		{
			int32_t bmin = 0, bmax = 0;
			if (!read_val(ptr, remaining, bmin)) return false;
			if (!read_val(ptr, remaining, bmax)) return false;
			if (!read_val(ptr, remaining, profile.brackets[b].multiplier)) return false;
			profile.brackets[b].bracket_min = bmin;
			profile.brackets[b].bracket_max = bmax;
		}

		add_scaling_profile(profile);
	}

	return true;
}

const std::unordered_map<std::string, scaling_profile>& formula_engine::get_scaling_profiles() const
{
	return m_scaling_profiles;
}

} // namespace hb::shared
