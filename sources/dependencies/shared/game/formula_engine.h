#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>

namespace hb::shared
{

// ============================================================================
// AST types for expression-based formulas
// ============================================================================

enum class ast_type { literal, variable, binary_op, unary_neg, func_call };

struct ast_node;
using ast_ptr = std::unique_ptr<ast_node>;

struct ast_node
{
	ast_type type = ast_type::literal;
	double value = 0.0;             // literal
	std::string name;               // variable or function name
	char op = 0;                    // +, -, *, /
	std::vector<ast_ptr> children;  // operands or function args
};

// A parsed formula with its expression string and AST
struct parsed_formula
{
	std::string formula_id;
	std::string expression;
	std::string description;
	ast_ptr root;
	std::unordered_set<std::string> required_vars; // variables found in AST
};

// Validation result from validate()
struct validation_result
{
	bool success = true;
	std::vector<std::string> errors;
	std::vector<std::string> warnings;
};

// Scaling bracket for a named profile
struct scaling_bracket
{
	int bracket_min = 0;
	int bracket_max = 0;
	double multiplier = 1.0;
};

struct scaling_profile
{
	std::string profile_id;
	std::vector<scaling_bracket> brackets;
};

// Stat-value map passed to evaluate()
using stat_map = std::unordered_map<std::string, double>;

class formula_engine
{
public:
	formula_engine() = default;

	// Clear all loaded formulas and scaling profiles
	void clear();

	// Add a formula from an expression string. Parses immediately.
	// Returns false if parse fails (error logged).
	bool add_formula(const std::string& id, const std::string& expression,
		const std::string& description);

	// Add a scaling profile
	void add_scaling_profile(const scaling_profile& profile);

	// Check if a formula exists
	bool has_formula(const std::string& formula_id) const;

	// Get the number of loaded formulas
	size_t formula_count() const;

	// Evaluate a formula with given stat values. Returns 0 if formula not found.
	// Cross-references other formulas when a variable isn't in stat_map.
	int evaluate(const std::string& formula_id, const stat_map& stats) const;

	// Validate all loaded formulas (parse checks, arg counts, circular refs, dry-run)
	validation_result validate() const;

	// Get all scaling profiles (for serialization)
	const std::unordered_map<std::string, scaling_profile>& get_scaling_profiles() const;

	// Serialization for client cache delivery (v2 format)
	std::vector<uint8_t> serialize() const;
	bool deserialize(const uint8_t* data, size_t size);

private:
	static constexpr int max_depth = 32;

	double evaluate_node(const ast_node* node, const stat_map& stats, int depth) const;
	double apply_scaling(const std::string& profile_id, double value) const;
	void collect_variables(const ast_node* node, std::unordered_set<std::string>& vars) const;

	std::unordered_map<std::string, parsed_formula> m_formulas;
	std::unordered_map<std::string, scaling_profile> m_scaling_profiles;
};

} // namespace hb::shared
